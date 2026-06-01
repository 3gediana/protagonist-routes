#include "world/SurvivalStatusLayer.h"

#include "brain/ProtagonistBrain.h"
#include "core/agent/Agent.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"
#include "core/world/layers/WeatherLayer.h"
#include "world/BaseLayer.h"
#include "world/CampfireLayer.h"
#include "world/WaterLayer.h"
#include "world/WorksiteLayer.h"

#include <algorithm>
#include <unordered_set>

namespace neuro::routes::protagonist {

namespace {

bool isProtagonist(const Agent& agent) {
    return dynamic_cast<const ProtagonistBrain*>(&agent.brain()) != nullptr;
}

}  // namespace

SurvivalStatusLayer::SurvivalStatusLayer(float max_health,
                                         float max_hydration,
                                         float initial_health_ratio,
                                         float initial_hydration_ratio,
                                         float hydration_decay_per_second,
                                         float dehydration_threshold_ratio,
                                         float dehydration_damage_per_second,
                                         float fire_health_damage_per_second,
                                         float warmth_recovery_per_second,
                                         float nest_recovery_per_second,
                                         float injured_threshold_ratio,
                                         bool track_all_living,
                                         float background_hydration_decay_per_second,
                                         float hunger_damage_threshold,
                                         float hunger_damage_per_second,
                                         float cold_temp_threshold,
                                         float hot_temp_threshold,
                                         float thermal_damage_per_second)
    : max_health_(std::max(1.0f, max_health)),
      max_hydration_(std::max(1.0f, max_hydration)),
      initial_health_ratio_(std::clamp(initial_health_ratio, 0.0f, 1.0f)),
      initial_hydration_ratio_(std::clamp(initial_hydration_ratio, 0.0f, 1.0f)),
      hydration_decay_per_second_(std::max(0.0f, hydration_decay_per_second)),
      dehydration_threshold_ratio_(std::clamp(dehydration_threshold_ratio, 0.0f, 1.0f)),
      dehydration_damage_per_second_(std::max(0.0f, dehydration_damage_per_second)),
      fire_health_damage_per_second_(std::max(0.0f, fire_health_damage_per_second)),
      warmth_recovery_per_second_(std::max(0.0f, warmth_recovery_per_second)),
      nest_recovery_per_second_(std::max(0.0f, nest_recovery_per_second)),
      injured_threshold_ratio_(std::clamp(injured_threshold_ratio, 0.0f, 1.0f)),
      track_all_living_(track_all_living),
      background_hydration_decay_per_second_(std::max(0.0f, background_hydration_decay_per_second)),
      hunger_damage_threshold_(std::clamp(hunger_damage_threshold, 0.0f, 1.0f)),
      hunger_damage_per_second_(std::max(0.0f, hunger_damage_per_second)),
      cold_temp_threshold_(cold_temp_threshold),
      hot_temp_threshold_(hot_temp_threshold),
      thermal_damage_per_second_(std::max(0.0f, thermal_damage_per_second)) {}

void SurvivalStatusLayer::onAttach(IWorld& world) {
    agents_ = world.getLayer<AgentLayer>();
    base_ = world.getLayer<BaseLayer>();
    campfire_ = world.getLayer<CampfireLayer>();
    water_ = world.getLayer<WaterLayer>();
}

void SurvivalStatusLayer::tick(IWorld& world, float dt) {
    syncTrackedAgents();
    if (agents_ == nullptr) {
        return;
    }

    const float dehydration_threshold = max_hydration_ * dehydration_threshold_ratio_;
    for (auto* agent : agents_->agents()) {
        if (agent == nullptr || !agent->alive()) {
            continue;
        }
        if (!track_all_living_ && !isProtagonist(*agent)) {
            continue;
        }

        auto it = states_.find(agent->id());
        if (it == states_.end()) {
            continue;
        }

        auto& state = it->second;

        // Hydration decay: protagonist uses primary rate, background
        // (Layer 1 R-001) uses separate slower rate (can be 0) to
        // avoid mass dehydration deaths since background has no
        // DrinkWaterCapability.
        const bool is_protag = isProtagonist(*agent);
        const float decay = is_protag ? hydration_decay_per_second_
                                      : background_hydration_decay_per_second_;
        // Layer 2: rain auto-recovers hydration (rain on skin / drinking
        // puddles). Query WeatherLayer; 0 when no rain.
        // Layer 5: agents inside a Shelter (yurt) are sheltered from rain
        // and therefore don't gain rain_recovery — encourages "build shelter
        // for yourself but stay outside in rain" tradeoff (rain pays back if
        // you don't have stockpiled water nearby).
        float rain_recovery = 0.0f;
        if (const auto* weather = world.getLayer<WeatherLayer>(); weather != nullptr) {
            rain_recovery = weather->hydrationRecoveryPerSecond();
            if (rain_recovery > 0.0f) {
                if (const auto* worksite = world.getLayer<WorksiteLayer>(); worksite != nullptr) {
                    if (worksite->isShelteredAt(agent->position())) {
                        rain_recovery = 0.0f;
                    }
                }
            }
        }
        state.hydration = std::clamp(state.hydration + (rain_recovery - decay) * dt,
                                     0.0f, max_hydration_);

        // Dehydration health damage (severity-continuous, R-001 "分阶段
        // 增强" — continuous severity ramp).
        if (state.hydration < dehydration_threshold && dehydration_threshold > 0.0f) {
            const float severity = 1.0f + (dehydration_threshold - state.hydration) / dehydration_threshold;
            state.health = std::max(0.0f, state.health - dehydration_damage_per_second_ * severity * dt);
        }

        // Layer 1 R-001: hunger-driven health damage. Severity ramps
        // continuously from threshold to 1.0.
        const float hunger = agent->driveState().hunger;
        if (hunger > hunger_damage_threshold_) {
            const float severity = (hunger - hunger_damage_threshold_)
                                   / std::max(0.01f, 1.0f - hunger_damage_threshold_);
            state.health = std::max(0.0f, state.health - hunger_damage_per_second_ * severity * dt);
        }

        // Layer 1 R-001: thermal damage (hypothermia + hyperthermia).
        // Severity ramps continuously from threshold outward.
        const float temp = agent->coreTemperature();
        if (temp < cold_temp_threshold_) {
            const float severity = (cold_temp_threshold_ - temp) / 10.0f;  // 1.0 at 10°C below threshold
            state.health = std::max(0.0f, state.health - thermal_damage_per_second_ * severity * dt);
        } else if (temp > hot_temp_threshold_) {
            const float severity = (temp - hot_temp_threshold_) / 5.0f;  // 1.0 at 5°C above threshold
            state.health = std::max(0.0f, state.health - thermal_damage_per_second_ * severity * dt);
        }

        // Fire / nest / warmth zones (protagonist-centric layers,
        // OK to apply to all tracked agents — background agents in
        // these zones get same effect, which is reasonable behaviour).
        if (campfire_ != nullptr && campfire_->inDangerZone(agent->position())) {
            state.health = std::max(0.0f, state.health - fire_health_damage_per_second_ * dt);
        } else if (base_ != nullptr && base_->inNest(agent->position())) {
            state.health = std::min(max_health_, state.health + nest_recovery_per_second_ * dt);
        } else if (campfire_ != nullptr && campfire_->inWarmthZone(agent->position()) && state.hydration >= dehydration_threshold) {
            state.health = std::min(max_health_, state.health + warmth_recovery_per_second_ * dt);
        }

        if (state.health <= 0.0f && agent->alive()) {
            // L4 (Final Blueprint) #3: classify death cause from the dominant
            // ongoing damage source at time-of-death so PopulationLayer's
            // per-cause death stats are meaningful. The four candidates that
            // can drive health to 0 in this layer are:
            //   - Dehydration (hydration < threshold)
            //   - Thermal (core_temperature outside cold/hot threshold)
            //   - Starvation (hunger > damage threshold)
            //   - Combat (campfire danger zone, modelled as fire-burn)
            // Pick whichever has the largest current severity.
            const float hydration_severity =
                (state.hydration < dehydration_threshold)
                ? (dehydration_threshold - state.hydration) / std::max(0.01f, dehydration_threshold)
                : 0.0f;
            const float current_temp = agent->coreTemperature();
            const float thermal_severity = current_temp < cold_temp_threshold_
                ? (cold_temp_threshold_ - current_temp) / 10.0f
                : (current_temp > hot_temp_threshold_
                    ? (current_temp - hot_temp_threshold_) / 5.0f
                    : 0.0f);
            const float hunger_severity =
                (agent->driveState().hunger > hunger_damage_threshold_)
                ? (agent->driveState().hunger - hunger_damage_threshold_)
                  / std::max(0.01f, 1.0f - hunger_damage_threshold_)
                : 0.0f;
            const bool in_fire = campfire_ != nullptr && campfire_->inDangerZone(agent->position());

            Agent::DeathCause cause = Agent::DeathCause::Unknown;
            float top = 0.0f;
            if (hydration_severity > top) { top = hydration_severity; cause = Agent::DeathCause::Dehydration; }
            if (thermal_severity  > top) { top = thermal_severity;  cause = Agent::DeathCause::Thermal; }
            if (hunger_severity   > top) { top = hunger_severity;   cause = Agent::DeathCause::Starvation; }
            if (in_fire)                 {                          cause = Agent::DeathCause::Combat; }
            agent->killWithCause(cause);

            world.eventBus().publish(events::Event{events::EventType::AgentDied,
                                                   events::AgentDied{agent->id(), "survival_status_depleted"},
                                                   world.currentTick(),
                                                   world.currentTimeSeconds()});
        }
    }
}

float SurvivalStatusLayer::hydrationRatio(AgentId id) const {
    const auto it = states_.find(id);
    if (it == states_.end()) {
        return 1.0f;
    }
    return std::clamp(it->second.hydration / max_hydration_, 0.0f, 1.0f);
}

float SurvivalStatusLayer::healthRatio(AgentId id) const {
    const auto it = states_.find(id);
    if (it == states_.end()) {
        return 1.0f;
    }
    return std::clamp(it->second.health / max_health_, 0.0f, 1.0f);
}

bool SurvivalStatusLayer::isDehydrated(AgentId id) const {
    return hydrationRatio(id) <= dehydration_threshold_ratio_;
}

bool SurvivalStatusLayer::isInjured(AgentId id) const {
    return healthRatio(id) <= injured_threshold_ratio_;
}

void SurvivalStatusLayer::drink(AgentId id, float amount) {
    const auto it = states_.find(id);
    if (it == states_.end()) {
        return;
    }
    it->second.hydration = std::clamp(it->second.hydration + amount, 0.0f, max_hydration_);
}

std::size_t SurvivalStatusLayer::trackedCount() const {
    return states_.size();
}

std::size_t SurvivalStatusLayer::dehydratedCount() const {
    std::size_t count = 0;
    for (const auto& [id, _] : states_) {
        count += isDehydrated(id) ? 1u : 0u;
    }
    return count;
}

std::size_t SurvivalStatusLayer::injuredCount() const {
    std::size_t count = 0;
    for (const auto& [id, _] : states_) {
        count += isInjured(id) ? 1u : 0u;
    }
    return count;
}

float SurvivalStatusLayer::averageHydrationRatio() const {
    if (states_.empty()) {
        return 1.0f;
    }
    float sum = 0.0f;
    for (const auto& [id, _] : states_) {
        sum += hydrationRatio(id);
    }
    return sum / static_cast<float>(states_.size());
}

float SurvivalStatusLayer::averageHealthRatio() const {
    if (states_.empty()) {
        return 1.0f;
    }
    float sum = 0.0f;
    for (const auto& [id, _] : states_) {
        sum += healthRatio(id);
    }
    return sum / static_cast<float>(states_.size());
}

void SurvivalStatusLayer::syncTrackedAgents() {
    if (agents_ == nullptr) {
        states_.clear();
        return;
    }

    std::unordered_set<AgentId> active_ids;
    for (auto* agent : agents_->agents()) {
        if (agent == nullptr || !agent->alive()) {
            continue;
        }
        // Layer 1 R-001: track all living entities by default (track_all_living_=true).
        // Set false to revert to protagonist-only behaviour (legacy mode).
        if (!track_all_living_ && !isProtagonist(*agent)) {
            continue;
        }
        active_ids.insert(agent->id());
        states_.try_emplace(agent->id(), SurvivalState{max_health_ * initial_health_ratio_, max_hydration_ * initial_hydration_ratio_});
    }

    for (auto it = states_.begin(); it != states_.end();) {
        if (!active_ids.contains(it->first)) {
            it = states_.erase(it);
        } else {
            ++it;
        }
    }
}

}  // namespace neuro::routes::protagonist
