#include "world/CampfireLayer.h"

#include "core/agent/Agent.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"
#include "core/world/layers/WeatherLayer.h"
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

// A fire is now a resource that must be maintained: lighting one grants a
// finite burn time, the fire consumes that fuel every tick, and it goes
// dormant when the fuel runs dry. Storms (rain/wind) eat fuel faster.
constexpr float kFuelPerIgnite = 150.0f;     // seconds of burn granted per ignition
constexpr float kFuelMax = 300.0f;           // cap so refuelling can't hoard infinitely
constexpr float kStormBurnMultiplier = 3.0f; // rain/wind burns fuel ~3x faster
// Realism (2026-06): fire should come from the protagonists' ignite skill,
// not appear on its own. Lightning can still occasionally start a fire (rare,
// realistic bootstrap when no one has learned to light one yet) but at a
// heavily reduced rate so agent-lit/refuelled fire is the dominant source.
constexpr float kLightningIgniteScale = 0.15f;

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

    // Layer 2 (FINAL_BLUEPRINT.md §2.3): lightning during storm/typhoon
    // can ignite a dormant fire. Probability per dormant fire per second
    // = WeatherLayer::lightningPerTreePerSecond() (we treat each dormant
    // fire as a "lightning candidate" — abstract: rather than modelling
    // tree-by-tree lightning strikes we let storm pulses occasionally
    // light up an existing campfire site).
    if (const auto* weather = world.getLayer<WeatherLayer>(); weather != nullptr) {
        const float p_per_sec = weather->lightningPerTreePerSecond();
        if (p_per_sec > 0.0f && dt > 0.0f) {
            std::uniform_real_distribution<float> u(0.0f, 1.0f);
            // Aggregate probability across all dormant fires; on hit,
            // ignite one. Roughly equivalent to per-fire sampling but
            // simpler.
            std::size_t dormant_count = 0;
            for (const auto& fire : fires_) {
                if (!fire.active) ++dormant_count;
            }
            const float total_p =
                p_per_sec * static_cast<float>(dormant_count) * dt * kLightningIgniteScale;
            if (total_p > 0.0f && u(rng_) < total_p) {
                igniteNextDormant();  // existing helper
            }
        }
    }

    // Fuel burn-down: every active fire consumes its fuel over time and goes
    // dormant when it runs dry. Storms (rain/wind) accelerate the burn. This
    // turns a campfire from a permanent fixture into a resource that has to
    // be lit and continually refuelled to keep its warmth zone alive.
    {
        float burn_mult = 1.0f;
        if (const auto* weather = world.getLayer<WeatherLayer>(); weather != nullptr) {
            if (weather->lightningPerTreePerSecond() > 0.0f) {
                burn_mult = kStormBurnMultiplier;
            }
        }
        for (auto& fire : fires_) {
            if (!fire.active) continue;
            fire.fuel -= dt * burn_mult;
            if (fire.fuel <= 0.0f) {
                fire.fuel = 0.0f;
                fire.active = false;
            }
        }
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
            // L4 #3: campfire burns count as Combat (fire-damage); not a
            // perfect taxonomy (there's no Burn cause) but Combat is
            // closer than Unknown.
            agent->killWithCause(Agent::DeathCause::Combat);
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
            fire.fuel = std::min(fire.fuel + kFuelPerIgnite, kFuelMax);
            return true;
        }
    }
    return false;
}

bool CampfireLayer::igniteOrRefuelNear(Vec2 position, float radius, float fuel_add) {
    // Pick the nearest fire site within radius (active or dormant) and pour
    // fuel into it, lighting it if it was out. This is how an agent "makes
    // fire": the IgniteFireCapability calls this when the agent spends a
    // stick at a fire site.
    CampfireSite* best = nullptr;
    float best_dist_sq = radius * radius;
    for (auto& fire : fires_) {
        const float dist_sq = Vec2::distanceSquared(fire.position, position);
        if (dist_sq <= best_dist_sq) {
            best = &fire;
            best_dist_sq = dist_sq;
        }
    }
    if (best == nullptr) {
        return false;
    }
    best->active = true;
    best->fuel = std::min(best->fuel + fuel_add, kFuelMax);
    return true;
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
