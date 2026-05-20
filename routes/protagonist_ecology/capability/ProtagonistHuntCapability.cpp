#include "capability/ProtagonistHuntCapability.h"

#include "brain/ProtagonistBrain.h"
#include "core/agent/Agent.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"

#include <cmath>

namespace neuro::routes::protagonist {

ProtagonistHuntCapability::ProtagonistHuntCapability(float hunt_radius, float damage, float energy_gain, float cost)
    : hunt_radius_(hunt_radius), damage_(damage), energy_gain_(energy_gain), cost_(cost) {}

void ProtagonistHuntCapability::apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) {
    if (outputs.empty() || outputs[0] < 0.3f) {
        return;
    }
    hunt_attempts.fetch_add(1, std::memory_order_relaxed);

    auto* concrete = dynamic_cast<Agent*>(&agent);
    if (concrete == nullptr) {
        return;
    }

    auto* agent_layer = world.getLayer<AgentLayer>();
    if (agent_layer == nullptr) {
        return;
    }

    concrete->setEnergy(concrete->energy() - cost_ * dt);

    for (auto* other : agent_layer->agents()) {
        if (other == nullptr || other == &agent || !other->alive()) {
            continue;
        }

        auto* target = dynamic_cast<Agent*>(other);
        if (target == nullptr || target->isPredator()) {
            continue;
        }
        if (dynamic_cast<const ProtagonistBrain*>(&target->brain()) != nullptr) {
            continue;
        }

        const float dx = agent.position().x - target->position().x;
        const float dy = agent.position().y - target->position().y;
        const float dist = std::sqrt(dx * dx + dy * dy);
        if (dist >= hunt_radius_) {
            continue;
        }

        const float actual_damage = damage_ * dt;
        const float actual_gain = energy_gain_ * dt;
        target->setEnergy(target->energy() - actual_damage);
        concrete->setEnergy(concrete->energy() + actual_gain);
        concrete->noteDamageDealt(actual_damage);
        if (actual_gain > 0.0f) {
            concrete->noteMeatFood(actual_gain);
        }
        if (target->energy() <= 0.0f) {
            target->kill("protagonist_predation");
            concrete->noteKill();
            hunt_successes.fetch_add(1, std::memory_order_relaxed);
        }
        break;
    }
}

}  // namespace neuro::routes::protagonist
