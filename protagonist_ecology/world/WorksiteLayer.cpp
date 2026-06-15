#include "world/WorksiteLayer.h"

#include "core/agent/Agent.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"
#include "core/world/layers/object/ObjectLayer.h"
#include "world/BaseLayer.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace neuro::routes::protagonist {

namespace {

Vec2 clampToWorld(Vec2 p, Vec2 world_size) {
    p.x = std::clamp(p.x, 0.0f, world_size.x);
    p.y = std::clamp(p.y, 0.0f, world_size.y);
    return p;
}

}  // namespace

WorksiteLayer::WorksiteLayer(std::size_t site_count,
                             float spawn_radius,
                             float interaction_radius,
                             std::size_t required_sticks,
                             std::size_t required_stones,
                             float assembly_rate,
                             float shelter_energy_bonus,
                             std::uint32_t seed)
    : site_count_(site_count),
      spawn_radius_(spawn_radius),
      interaction_radius_(interaction_radius),
      required_sticks_(required_sticks),
      required_stones_(required_stones),
      assembly_rate_(assembly_rate),
      shelter_energy_bonus_(shelter_energy_bonus),
      rng_(seed == 0 ? std::random_device{}() : seed) {}

void WorksiteLayer::setBuildingTypeWeights(float wall, float shelter, float storage, float lookout) {
    const float total = wall + shelter + storage + lookout;
    if (total <= 0.0f) {
        building_type_weights_ = {1.0f, 0.0f, 0.0f, 0.0f};
        return;
    }
    building_type_weights_ = {
        wall / total, shelter / total, storage / total, lookout / total
    };
}

void WorksiteLayer::onAttach(IWorld& world) {
    base_ = world.getLayer<BaseLayer>();
    agents_ = world.getLayer<AgentLayer>();
    objects_ = world.getLayer<ObjectLayer>();
    seedSites(world.size());
}

void WorksiteLayer::tick(IWorld& world, float dt) {
    absorbCarriedMaterials(world);
    absorbNearbyMaterials();
    progressConstruction(world, dt);
    // Layer 5 R2: completed Storage buildings keep absorbing.
    absorbToCompletedStorages(world);
    // Layer 5 R3: update per-agent sense-radius scale based on Lookout proximity.
    if (agents_ != nullptr) {
        for (auto* a : agents_->agents()) {
            if (a == nullptr || !a->alive()) continue;
            if (buildingAt(a->position(), BuildingType::Lookout).has_value()) {
                a->setSenseRadiusScale(1.5f);
            } else {
                a->setSenseRadiusScale(1.0f);
            }
        }
    }
    // Emergent shelter L1: completed worksites act as collidable walls
    // for predators. Runs after AgentLayer (tickOrder 100 < 155) so the
    // movement positions for this tick are already in agent->position()
    // and we can push them back out to the wall boundary.
    enforcePredatorObstacles();
}

std::optional<Worksite> WorksiteLayer::nearestActiveSite(Vec2 position) const {
    const Worksite* best = nullptr;
    float best_dist_sq = 0.0f;
    for (const auto& site : sites_) {
        if (!site.active) {
            continue;
        }
        const float dist_sq = Vec2::distanceSquared(site.position, position);
        if (best == nullptr || dist_sq < best_dist_sq) {
            best = &site;
            best_dist_sq = dist_sq;
        }
    }
    return best == nullptr ? std::nullopt : std::optional<Worksite>(*best);
}

bool WorksiteLayer::inActiveSite(Vec2 position) const {
    for (const auto& site : sites_) {
        if (!site.active) {
            continue;
        }
        if (Vec2::distanceSquared(site.position, position) <= site.radius * site.radius) {
            return true;
        }
    }
    return false;
}

bool WorksiteLayer::activateNextDormant() {
    for (auto& site : sites_) {
        if (!site.active) {
            site.active = true;
            // L5 #6: per-activation protagonist vote override. If any
            // protagonist (archetype_tag==0) is within 3x interaction_radius
            // of this site, take the modal buildingTypeChoice() over them
            // and override the seeded type. Falls back to the seeded
            // distribution when no protagonists are nearby.
            if (agents_ != nullptr) {
                int votes[4] = {0, 0, 0, 0};
                int total = 0;
                const float vote_radius = interaction_radius_ * 3.0f;
                const float r2 = vote_radius * vote_radius;
                for (const auto* a : agents_->agents()) {
                    if (a == nullptr || !a->alive() || a->archetypeTag() != 0) continue;
                    if (Vec2::distanceSquared(a->position(), site.position) > r2) continue;
                    const auto bt = a->buildingTypeChoice();
                    if (bt < 4) {
                        ++votes[bt];
                        ++total;
                    }
                }
                if (total > 0) {
                    int best = 0;
                    for (int i = 1; i < 4; ++i) {
                        if (votes[i] > votes[best]) best = i;
                    }
                    site.building_type = static_cast<BuildingType>(best);
                }
            }
            return true;
        }
    }
    return false;
}

std::size_t WorksiteLayer::activeSiteCount() const {
    std::size_t count = 0;
    for (const auto& site : sites_) {
        count += site.active ? 1u : 0u;
    }
    return count;
}

std::size_t WorksiteLayer::completedSiteCount() const {
    std::size_t count = 0;
    for (const auto& site : sites_) {
        count += site.completed ? 1u : 0u;
    }
    return count;
}

bool WorksiteLayer::blocksPredatorAt(Vec2 pos) const {
    for (const auto& site : sites_) {
        if (!site.completed) {
            continue;
        }
        // Layer 5: only Wall-type buildings block predators.
        if (site.building_type != BuildingType::Wall) continue;
        if (Vec2::distanceSquared(site.position, pos) < site.radius * site.radius) {
            return true;
        }
    }
    return false;
}

const char* buildingTypeName(BuildingType t) {
    switch (t) {
        case BuildingType::Wall:    return "wall";
        case BuildingType::Shelter: return "shelter";
        case BuildingType::Storage: return "storage";
        case BuildingType::Lookout: return "lookout";
    }
    return "unknown";
}

std::optional<Worksite> WorksiteLayer::buildingAt(Vec2 pos, BuildingType type) const {
    for (const auto& site : sites_) {
        if (!site.completed) continue;
        if (site.building_type != type) continue;
        if (Vec2::distanceSquared(site.position, pos) < site.radius * site.radius) {
            return site;
        }
    }
    return std::nullopt;
}

bool WorksiteLayer::isShelteredAt(Vec2 pos) const {
    return buildingAt(pos, BuildingType::Shelter).has_value();
}

std::size_t WorksiteLayer::totalStorageSticks() const {
    std::size_t total = 0;
    for (const auto& s : sites_) {
        if (s.completed && s.building_type == BuildingType::Storage) {
            total += s.stored_sticks_overflow;
        }
    }
    return total;
}

std::size_t WorksiteLayer::totalStorageStones() const {
    std::size_t total = 0;
    for (const auto& s : sites_) {
        if (s.completed && s.building_type == BuildingType::Storage) {
            total += s.stored_stones_overflow;
        }
    }
    return total;
}

void WorksiteLayer::absorbToCompletedStorages(IWorld& world) {
    if (objects_ == nullptr || agents_ == nullptr) return;

    const double now = world.currentTimeSeconds();
    const Tick now_tick = world.currentTick();

    for (auto& site : sites_) {
        if (!site.completed || site.building_type != BuildingType::Storage) continue;

        for (auto* agent : agents_->agents()) {
            if (agent == nullptr || !agent->alive() || !agent->isCarrying()) continue;
            if (Vec2::distanceSquared(agent->position(), site.position) > site.radius * site.radius) continue;

            auto* object = objects_->findById(agent->carriedObject());
            if (object == nullptr) {
                agent->setCarriedObject(0);
                continue;
            }

            std::uint8_t material_type = 0;
            if (object->type == ObjectType::Stick) {
                ++site.stored_sticks_overflow;
                material_type = static_cast<std::uint8_t>(ObjectType::Stick);
            } else if (object->type == ObjectType::Stone) {
                ++site.stored_stones_overflow;
                material_type = static_cast<std::uint8_t>(ObjectType::Stone);
            } else {
                continue;
            }

            object->velocity = kZeroVec2;
            object->pos = site.position;
            object->carried = true;
            const AgentId depositor = agent->id();
            agent->setCarriedObject(0);

            world.eventBus().publish(events::Event{
                events::EventType::WorksiteMaterialDeposited,
                events::WorksiteMaterialDeposited{depositor, site.id, material_type, now},
                now_tick,
                now});
        }
    }
}

void WorksiteLayer::spawnCompletedAt(Vec2 position) {
    Worksite site{};
    site.id = sites_.size() + 1u;
    site.position = position;
    site.radius = interaction_radius_;
    site.active = true;
    site.completed = true;
    site.stored_sticks = required_sticks_;
    site.stored_stones = required_stones_;
    site.required_sticks = required_sticks_;
    site.required_stones = required_stones_;
    site.build_progress = 1.0f;
    site.build_started_at_seconds = -1.0;
    sites_.push_back(std::move(site));
}

void WorksiteLayer::enforcePredatorObstacles() {
    if (agents_ == nullptr) {
        return;
    }
    for (auto* agent : agents_->agents()) {
        if (agent == nullptr || !agent->alive()) {
            continue;
        }
        const auto* concrete = dynamic_cast<const Agent*>(agent);
        if (concrete == nullptr || !concrete->isPredator()) {
            continue;
        }
        for (const auto& site : sites_) {
            if (!site.completed) {
                continue;
            }
            const Vec2 to_agent = agent->position() - site.position;
            const float dist = to_agent.length();
            if (dist >= site.radius) {
                continue;
            }
            // Predator stepped into a completed worksite (a built wall).
            // Push it out to the boundary along the radial direction so it
            // physically cannot walk through. Velocity is zeroed so it does
            // not keep ramming next tick (it will need to re-decide a move
            // direction). Protagonists / foragers / non-predators are not
            // affected, so the protagonist can still enter the nest area
            // and reach the shelter_energy_bonus tile inside.
            const Vec2 push_dir = (dist > 0.0001f)
                                      ? (to_agent / dist)
                                      : Vec2{1.0f, 0.0f};
            agent->setPosition(site.position + push_dir * (site.radius + 0.05f));
            agent->setVelocity(Vec2{0.0f, 0.0f});
            break;  // already pushed out for this tick
        }
    }
}

void WorksiteLayer::seedSites(Vec2 world_size) {
    sites_.clear();
    if (site_count_ == 0) {
        return;
    }

    std::uniform_real_distribution<float> angle_dist(0.0f, 6.28318530718f);
    std::uniform_real_distribution<float> radius_dist(spawn_radius_ * 0.8f, spawn_radius_ * 1.35f);
    const Vec2 anchor = base_ != nullptr ? base_->baseCenter() : Vec2{world_size.x * 0.5f, world_size.y * 0.5f};
    sites_.reserve(site_count_);
    for (std::size_t i = 0; i < site_count_; ++i) {
        Vec2 position{};
        if (base_ != nullptr) {
            Vec2 corridor_dir = base_->stockpileCenter() - base_->baseCenter();
            float corridor_len = corridor_dir.length();
            if (corridor_len <= 0.0001f) {
                corridor_dir = Vec2{1.0f, 0.0f};
                corridor_len = 1.0f;
            }
            corridor_dir /= corridor_len;
            const Vec2 corridor_normal{-corridor_dir.y, corridor_dir.x};
            std::uniform_real_distribution<float> forward_dist(spawn_radius_ * 0.55f, spawn_radius_ * 0.95f);
            std::uniform_real_distribution<float> lateral_dist(-spawn_radius_ * 0.35f, spawn_radius_ * 0.35f);
            position = base_->stockpileCenter() + corridor_dir * forward_dist(rng_) + corridor_normal * lateral_dist(rng_);
        } else {
            const float angle = angle_dist(rng_);
            const float radius = radius_dist(rng_);
            position = anchor + Vec2{std::cos(angle) * radius, std::sin(angle) * radius};
        }
        Worksite ws{};
        ws.id = i + 1u;
        ws.position = clampToWorld(position, world_size);
        ws.radius = interaction_radius_;
        ws.active = false;
        ws.completed = false;
        // L5 R2: sample building_type from configured distribution.
        std::uniform_real_distribution<float> u(0.0f, 1.0f);
        const float roll = u(rng_);
        float cum = 0.0f;
        BuildingType chosen = BuildingType::Wall;
        for (std::size_t bt = 0; bt < 4; ++bt) {
            cum += building_type_weights_[bt];
            if (roll <= cum) {
                chosen = static_cast<BuildingType>(bt);
                break;
            }
        }
        ws.building_type = chosen;
        ws.stored_sticks = 0;
        ws.stored_stones = 0;
        ws.required_sticks = required_sticks_;
        ws.required_stones = required_stones_;
        ws.build_progress = 0.0f;
        sites_.push_back(std::move(ws));
    }
}

void WorksiteLayer::absorbCarriedMaterials(IWorld& world) {
    if (sites_.empty() || objects_ == nullptr || agents_ == nullptr) {
        return;
    }

    const double now = world.currentTimeSeconds();
    const Tick now_tick = world.currentTick();

    for (auto& site : sites_) {
        if (!site.active || site.completed) {
            continue;
        }

        for (auto* agent : agents_->agents()) {
            if (agent == nullptr || !agent->alive() || !agent->isCarrying()) {
                continue;
            }
            if (Vec2::distanceSquared(agent->position(), site.position) > site.radius * site.radius) {
                continue;
            }

            auto* object = objects_->findById(agent->carriedObject());
            if (object == nullptr) {
                agent->setCarriedObject(0);
                continue;
            }

            bool consumed = false;
            std::uint8_t material_type = 0;
            if (object->type == ObjectType::Stick && site.stored_sticks < site.required_sticks) {
                ++site.stored_sticks;
                consumed = true;
                material_type = static_cast<std::uint8_t>(ObjectType::Stick);
            } else if (object->type == ObjectType::Stone && site.stored_stones < site.required_stones) {
                ++site.stored_stones;
                consumed = true;
                material_type = static_cast<std::uint8_t>(ObjectType::Stone);
            }
            if (!consumed) {
                continue;
            }

            object->velocity = kZeroVec2;
            object->pos = site.position;
            object->carried = true;
            const AgentId depositor = agent->id();
            agent->setCarriedObject(0);

            world.eventBus().publish(events::Event{
                events::EventType::WorksiteMaterialDeposited,
                events::WorksiteMaterialDeposited{depositor, site.id, material_type, now},
                now_tick,
                now});
        }
    }
}

void WorksiteLayer::absorbNearbyMaterials() {
    if (sites_.empty() || objects_ == nullptr) {
        return;
    }

    for (auto& site : sites_) {
        if (!site.active || site.completed) {
            continue;
        }

        for (auto& object : objects_->objects()) {
            if (object.carried) {
                continue;
            }
            if (Vec2::distanceSquared(object.pos, site.position) > site.radius * site.radius) {
                continue;
            }

            if (object.type == ObjectType::Stick && site.stored_sticks < site.required_sticks) {
                ++site.stored_sticks;
            } else if (object.type == ObjectType::Stone && site.stored_stones < site.required_stones) {
                ++site.stored_stones;
            } else {
                continue;
            }

            object.velocity = kZeroVec2;
            object.pos = site.position;
            object.carried = true;
        }
    }
}

void WorksiteLayer::progressConstruction(IWorld& world, float dt) {
    auto* agent_layer = world.getLayer<AgentLayer>();
    if (agent_layer == nullptr) {
        return;
    }

    const double now = world.currentTimeSeconds();
    const Tick now_tick = world.currentTick();

    for (auto& site : sites_) {
        if (!site.active || site.completed) {
            continue;
        }
        if (site.stored_sticks < site.required_sticks || site.stored_stones < site.required_stones) {
            continue;
        }

        // Snapshot present-this-tick set and emit BuildStarted on first entry
        // for any new builder. This is per-agent (not per-site) so memory and
        // ledger know who joined when.
        std::unordered_set<AgentId> present_now;
        present_now.reserve(8);
        for (const auto* agent : agent_layer->agents()) {
            if (agent == nullptr || !agent->alive()) {
                continue;
            }
            if (Vec2::distanceSquared(agent->position(), site.position) > site.radius * site.radius) {
                continue;
            }
            present_now.insert(agent->id());
            if (site.active_builders.find(agent->id()) == site.active_builders.end()) {
                site.active_builders.insert(agent->id());
                world.eventBus().publish(events::Event{
                    events::EventType::BuildStarted,
                    events::BuildStarted{agent->id(), site.id, site.position, now},
                    now_tick,
                    now});
            }
        }
        // Drop builders who left the radius. We do not emit a "BuildLeft"
        // event right now (their accumulated time stays in the ledger map),
        // but if needed later it can be added here.
        for (auto it = site.active_builders.begin(); it != site.active_builders.end();) {
            if (present_now.find(*it) == present_now.end()) {
                it = site.active_builders.erase(it);
            } else {
                ++it;
            }
        }

        const std::size_t nearby_workers = present_now.size();
        if (nearby_workers == 0) {
            continue;
        }

        // Per-agent contribution accumulation: each present agent gets dt
        // seconds of credit on this site. Used by BuildCompleted contributors
        // payload and by the credit-assignment ledger (P3).
        for (AgentId id : present_now) {
            site.per_agent_contribution_seconds[id] += dt;
        }

        if (site.build_started_at_seconds < 0.0) {
            site.build_started_at_seconds = now;
        }

        site.build_progress += static_cast<float>(nearby_workers) * assembly_rate_ * dt;
        if (site.build_progress >= 1.0f) {
            site.build_progress = 1.0f;
            site.completed = true;

            // Build the contributor list ordered by contribution time desc
            // so consumers (memory / ledger) see the dominant builders first.
            std::vector<std::pair<AgentId, float>> sorted;
            sorted.reserve(site.per_agent_contribution_seconds.size());
            for (const auto& [aid, secs] : site.per_agent_contribution_seconds) {
                sorted.emplace_back(aid, secs);
            }
            std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
                return a.second > b.second;
            });
            std::vector<AgentId> contributors;
            std::vector<float> contribution_seconds;
            contributors.reserve(sorted.size());
            contribution_seconds.reserve(sorted.size());
            for (const auto& [aid, secs] : sorted) {
                contributors.push_back(aid);
                contribution_seconds.push_back(secs);
            }
            const double duration = site.build_started_at_seconds >= 0.0
                                        ? now - site.build_started_at_seconds
                                        : 0.0;
            world.eventBus().publish(events::Event{
                events::EventType::BuildCompleted,
                events::BuildCompleted{
                    site.id,
                    site.position,
                    std::move(contributors),
                    std::move(contribution_seconds),
                    site.build_started_at_seconds,
                    duration},
                now_tick,
                now});
        }
    }

    for (auto* agent : agent_layer->agents()) {
        if (agent == nullptr || !agent->alive()) {
            continue;
        }
        for (const auto& site : sites_) {
            if (!site.completed) {
                continue;
            }
            if (Vec2::distanceSquared(agent->position(), site.position) <= site.radius * site.radius) {
                const float previous = agent->energy();
                const float next = previous + shelter_energy_bonus_ * dt;
                if (next != previous) {
                    agent->setEnergy(next);
                    world.eventBus().publish(events::Event{events::EventType::AgentEnergyChanged,
                                                           events::AgentEnergyChanged{agent->id(), previous, next},
                                                           world.currentTick(),
                                                           world.currentTimeSeconds()});
                }
            }
        }
    }
}

}  // namespace neuro::routes::protagonist
