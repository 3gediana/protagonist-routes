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

void WorksiteLayer::onAttach(IWorld& world) {
    base_ = world.getLayer<BaseLayer>();
    agents_ = world.getLayer<AgentLayer>();
    objects_ = world.getLayer<ObjectLayer>();
    seedSites(world.size());
}

void WorksiteLayer::tick(IWorld& world, float dt) {
    absorbCarriedMaterials();
    absorbNearbyMaterials();
    progressConstruction(world, dt);
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
        sites_.push_back(Worksite{
            i + 1u,
            clampToWorld(position, world_size),
            interaction_radius_,
            false,
            false,
            0,
            0,
            required_sticks_,
            required_stones_,
            0.0f});
    }
}

void WorksiteLayer::absorbCarriedMaterials() {
    if (sites_.empty() || objects_ == nullptr || agents_ == nullptr) {
        return;
    }

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
            if (object->type == ObjectType::Stick && site.stored_sticks < site.required_sticks) {
                ++site.stored_sticks;
                consumed = true;
            } else if (object->type == ObjectType::Stone && site.stored_stones < site.required_stones) {
                ++site.stored_stones;
                consumed = true;
            }
            if (!consumed) {
                continue;
            }

            object->velocity = kZeroVec2;
            object->pos = site.position;
            object->carried = true;
            agent->setCarriedObject(0);
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

    for (auto& site : sites_) {
        if (!site.active || site.completed) {
            continue;
        }
        if (site.stored_sticks < site.required_sticks || site.stored_stones < site.required_stones) {
            continue;
        }

        std::size_t nearby_workers = 0;
        for (const auto* agent : agent_layer->agents()) {
            if (agent == nullptr || !agent->alive()) {
                continue;
            }
            if (Vec2::distanceSquared(agent->position(), site.position) <= site.radius * site.radius) {
                ++nearby_workers;
            }
        }

        if (nearby_workers == 0) {
            continue;
        }

        site.build_progress += static_cast<float>(nearby_workers) * assembly_rate_ * dt;
        if (site.build_progress >= 1.0f) {
            site.build_progress = 1.0f;
            site.completed = true;
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
