#include "core/capability/FleeCapability.h"
#include "core/agent/Agent.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"
#include "core/world/layers/TimeOfDayLayer.h"

#include <cmath>

namespace neuro {

FleeCapability::FleeCapability(float flee_radius, float flee_speed_boost, float flee_cost,
                               TwoStageFleeParams params)
    : flee_radius_(flee_radius),
      flee_speed_boost_(flee_speed_boost),
      flee_cost_(flee_cost),
      params_(params) {}

std::size_t FleeCapability::consumedOutputs() const {
    return 1;  // flee intensity [0,1]
}

void FleeCapability::apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) {
    if (outputs.empty() || outputs[0] < 0.3f) return;

    auto* concrete = dynamic_cast<Agent*>(&agent);
    if (concrete == nullptr) return;
    if (concrete->isPredator()) return;  // predators don't flee

    auto* agent_layer = world.getLayer<AgentLayer>();
    if (agent_layer == nullptr) return;

    const bool two_stage = params_.enabled;
    const SimTimeSeconds now = world.currentTimeSeconds();
    // Two-stage mode: protagonist-like hunters (archetype tag 0) only
    // count as threats while the prey was damaged recently. Legacy mode
    // (two_stage == false) keeps the predator-only threat set.
    const bool hunters_are_threats =
        two_stage && concrete->lastDamageTimeSeconds() >= 0.0
        && (now - concrete->lastDamageTimeSeconds())
               <= static_cast<SimTimeSeconds>(params_.damage_memory_seconds);

    // Find nearest predator
    float nearest_dx = 0.0f;
    float nearest_dy = 0.0f;
    float nearest_dist = flee_radius_;
    bool threat_found = false;

    for (const auto* other : agent_layer->agents()) {
        if (other == &agent || !other->alive()) continue;
        const auto* other_concrete = dynamic_cast<const Agent*>(other);
        if (other_concrete == nullptr) continue;
        const bool is_predator = other_concrete->isPredator();
        if (!is_predator && !(hunters_are_threats && other_concrete->archetypeTag() == 0)) continue;

        const float dx = agent.position().x - other->position().x;
        const float dy = agent.position().y - other->position().y;
        const float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < nearest_dist) {
            nearest_dist = dist;
            nearest_dx = dx;
            nearest_dy = dy;
            threat_found = true;
        }
    }

    if (!threat_found) {
        if (two_stage) cycle_active_ = false;
        return;
    }

    // Two-stage burst/rest cycle. Starts on the first threatened tick;
    // while threatened it alternates burst_seconds of fleeing with
    // rest_seconds of no flee movement. Threat-free ticks reset the
    // cycle so the next hit starts with a fresh burst.
    if (two_stage) {
        if (!cycle_active_) {
            cycle_active_ = true;
            resting_ = false;
            phase_started_ = now;
        } else {
            const float phase_len = resting_ ? params_.rest_seconds : params_.burst_seconds;
            if ((now - phase_started_) >= static_cast<SimTimeSeconds>(phase_len)) {
                resting_ = !resting_;
                phase_started_ = now;
            }
        }
        if (resting_) return;  // respite window: no flee movement
    }

    // Day/night: prey flee more effectively during day (1.0x), worse at night (0.6x)
    float time_mult = 1.0f;
    const auto* tod = world.getLayer<TimeOfDayLayer>();
    if (tod != nullptr) {
        const float light = tod->lightLevel();
        time_mult = 0.6f + 0.4f * light;
    }

    // Flee direction: away from predator
    const float len = std::sqrt(nearest_dx * nearest_dx + nearest_dy * nearest_dy);
    if (len < 0.001f) return;

    const float flee_intensity = outputs[0];
    const float boost = flee_speed_boost_ * flee_intensity * time_mult;
    Vec2 flee_dir{nearest_dx / len, nearest_dy / len};

    // Apply flee velocity boost on top of normal movement
    Vec2 flee_velocity = flee_dir * boost;
    const Vec2 previous_position = agent.position();
    const float previous_energy = concrete->energy();
    Vec2 new_pos = agent.position() + flee_velocity * dt;

    // Clamp to world bounds
    const Vec2 ws = world.size();
    new_pos.x = std::clamp(new_pos.x, 0.0f, ws.x);
    new_pos.y = std::clamp(new_pos.y, 0.0f, ws.y);

    agent.setPosition(new_pos);
    concrete->setEnergy(concrete->energy() - flee_cost_ * flee_intensity * dt);
    world.eventBus().publish(events::Event{events::EventType::AgentMoved,
                                           events::AgentMoved{agent.id(), previous_position, new_pos},
                                           world.currentTick(),
                                           world.currentTimeSeconds()});
    world.eventBus().publish(events::Event{events::EventType::AgentFled,
                                           events::AgentFled{agent.id(), previous_position, new_pos, flee_intensity},
                                           world.currentTick(),
                                           world.currentTimeSeconds()});
    world.eventBus().publish(events::Event{events::EventType::AgentEnergyChanged,
                                           events::AgentEnergyChanged{agent.id(), previous_energy, concrete->energy()},
                                           world.currentTick(),
                                           world.currentTimeSeconds()});
}

const char* FleeCapability::name() const {
    return "flee";
}

}  // namespace neuro
