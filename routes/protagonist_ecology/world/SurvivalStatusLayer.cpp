#include "world/SurvivalStatusLayer.h"

#include "brain/ProtagonistBrain.h"
#include "core/agent/Agent.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"
#include "world/BaseLayer.h"
#include "world/CampfireLayer.h"
#include "world/WaterLayer.h"

#include <algorithm>
#include <unordered_set>

namespace neuro::routes::protagonist {

namespace {

bool shouldTrackAgent(const Agent& agent) {
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
                                         float injured_threshold_ratio)
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
      injured_threshold_ratio_(std::clamp(injured_threshold_ratio, 0.0f, 1.0f)) {}

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
        if (agent == nullptr || !agent->alive() || !shouldTrackAgent(*agent)) {
            continue;
        }

        auto it = states_.find(agent->id());
        if (it == states_.end()) {
            continue;
        }

        auto& state = it->second;
        state.hydration = std::clamp(state.hydration - hydration_decay_per_second_ * dt, 0.0f, max_hydration_);

        if (state.hydration < dehydration_threshold && dehydration_threshold > 0.0f) {
            const float severity = 1.0f + (dehydration_threshold - state.hydration) / dehydration_threshold;
            state.health = std::max(0.0f, state.health - dehydration_damage_per_second_ * severity * dt);
        }

        if (campfire_ != nullptr && campfire_->inDangerZone(agent->position())) {
            state.health = std::max(0.0f, state.health - fire_health_damage_per_second_ * dt);
        } else if (base_ != nullptr && base_->inNest(agent->position())) {
            state.health = std::min(max_health_, state.health + nest_recovery_per_second_ * dt);
        } else if (campfire_ != nullptr && campfire_->inWarmthZone(agent->position()) && state.hydration >= dehydration_threshold) {
            state.health = std::min(max_health_, state.health + warmth_recovery_per_second_ * dt);
        }

        if (state.health <= 0.0f && agent->alive()) {
            agent->kill("survival_status_depleted");
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
        if (agent == nullptr || !agent->alive() || !shouldTrackAgent(*agent)) {
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
