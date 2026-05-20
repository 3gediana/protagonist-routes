#include "world/CampfireLayer.h"

#include "core/agent/Agent.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"
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

CampfireLayer::CampfireLayer(std::size_t fire_count,
                             float spawn_radius,
                             float warmth_radius,
                             float danger_radius,
                             float warmth_energy_bonus,
                             float danger_damage,
                             std::uint32_t seed)
    : fire_count_(fire_count),
      spawn_radius_(spawn_radius),
      warmth_radius_(warmth_radius),
      danger_radius_(danger_radius),
      warmth_energy_bonus_(warmth_energy_bonus),
      danger_damage_(danger_damage),
      rng_(seed == 0 ? std::random_device{}() : seed) {}

void CampfireLayer::onAttach(IWorld& world) {
    base_ = world.getLayer<BaseLayer>();
    agents_ = world.getLayer<AgentLayer>();
    seedFires(world.size());
}

void CampfireLayer::tick(IWorld& world, float dt) {
    if (agents_ == nullptr || fires_.empty()) {
        return;
    }

    for (Agent* agent : agents_->agents()) {
        if (agent == nullptr || !agent->alive()) {
            continue;
        }

        const auto nearest = nearestFire(agent->position());
        if (!nearest.has_value()) {
            continue;
        }

        const float dist = Vec2::distance(nearest->position, agent->position());
        const float previous = agent->energy();
        float next = previous;
        if (dist <= danger_radius_) {
            next -= danger_damage_ * nearest->intensity * dt;
        } else if (dist <= warmth_radius_) {
            next += warmth_energy_bonus_ * nearest->intensity * dt;
        }

        if (next != previous) {
            agent->setEnergy(next);
            world.eventBus().publish(events::Event{events::EventType::AgentEnergyChanged,
                                                   events::AgentEnergyChanged{agent->id(), previous, next},
                                                   world.currentTick(),
                                                   world.currentTimeSeconds()});
        }

        if (agent->energy() <= 0.0f && agent->alive()) {
            agent->kill("campfire_burn");
            world.eventBus().publish(events::Event{events::EventType::AgentDied,
                                                   events::AgentDied{agent->id(), "campfire_burn"},
                                                   world.currentTick(),
                                                   world.currentTimeSeconds()});
        }
    }
}

std::optional<CampfireSite> CampfireLayer::nearestFire(Vec2 position) const {
    if (fires_.empty()) {
        return std::nullopt;
    }

    const CampfireSite* best = nullptr;
    float best_dist_sq = 0.0f;
    for (const auto& fire : fires_) {
        if (!fire.active) {
            continue;
        }
        const float dist_sq = Vec2::distanceSquared(fire.position, position);
        if (best == nullptr || dist_sq < best_dist_sq) {
            best = &fire;
            best_dist_sq = dist_sq;
        }
    }
    return best == nullptr ? std::nullopt : std::optional<CampfireSite>(*best);
}

bool CampfireLayer::inWarmthZone(Vec2 position) const {
    for (const auto& fire : fires_) {
        if (!fire.active) {
            continue;
        }
        if (Vec2::distanceSquared(fire.position, position) <= warmth_radius_ * warmth_radius_) {
            return true;
        }
    }
    return false;
}

bool CampfireLayer::inDangerZone(Vec2 position) const {
    for (const auto& fire : fires_) {
        if (!fire.active) {
            continue;
        }
        if (Vec2::distanceSquared(fire.position, position) <= danger_radius_ * danger_radius_) {
            return true;
        }
    }
    return false;
}

bool CampfireLayer::igniteNextDormant() {
    for (auto& fire : fires_) {
        if (!fire.active) {
            fire.active = true;
            return true;
        }
    }
    return false;
}

std::size_t CampfireLayer::activeFireCount() const {
    std::size_t count = 0;
    for (const auto& fire : fires_) {
        count += fire.active ? 1u : 0u;
    }
    return count;
}

void CampfireLayer::seedFires(Vec2 world_size) {
    fires_.clear();
    if (fire_count_ == 0) {
        return;
    }

    std::uniform_real_distribution<float> angle_dist(0.0f, 6.28318530718f);
    std::uniform_real_distribution<float> radial_dist(std::max(danger_radius_ * 2.0f, 3.0f), std::max(spawn_radius_, danger_radius_ * 2.5f));
    std::uniform_real_distribution<float> intensity_dist(0.85f, 1.15f);
    std::uniform_real_distribution<float> x_dist(0.0f, world_size.x);
    std::uniform_real_distribution<float> y_dist(0.0f, world_size.y);

    fires_.reserve(fire_count_);
    const Vec2 anchor = base_ != nullptr ? base_->baseCenter() : Vec2{x_dist(rng_), y_dist(rng_)};
    for (std::size_t i = 0; i < fire_count_; ++i) {
        const float angle = angle_dist(rng_);
        const float radius = radial_dist(rng_);
        const Vec2 offset{std::cos(angle) * radius, std::sin(angle) * radius};
        fires_.push_back(CampfireSite{i + 1u, clampToWorld(anchor + offset, world_size), intensity_dist(rng_), false});
    }
}

}  // namespace neuro::routes::protagonist
