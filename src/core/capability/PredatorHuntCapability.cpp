#include "core/capability/PredatorHuntCapability.h"

#include "core/agent/Agent.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"

#include <cmath>

namespace neuro {

PredatorHuntCapability::PredatorHuntCapability(float hunt_radius, float damage, float energy_gain, float cost)
    : hunt_radius_(hunt_radius), damage_(damage), energy_gain_(energy_gain), cost_(cost) {}

std::size_t PredatorHuntCapability::consumedOutputs() const {
    return 1;
}

void PredatorHuntCapability::apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) {
    if (outputs.empty() || outputs[0] < 0.3f) return;

    auto* concrete = dynamic_cast<Agent*>(&agent);
    if (concrete == nullptr) return;

    auto* agent_layer = world.getLayer<AgentLayer>();
    if (agent_layer == nullptr) return;

    concrete->setEnergy(concrete->energy() - cost_ * dt);

    for (auto* other : agent_layer->agents()) {
        if (other == &agent || !other->alive()) continue;

        const auto* other_concrete = dynamic_cast<const Agent*>(other);
        if (other_concrete == nullptr) continue;

        if (other_concrete->isPredator()) continue;

        const float dx = agent.position().x - other->position().x;
        const float dy = agent.position().y - other->position().y;
        const float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < hunt_radius_) {
            other->setEnergy(other->energy() - damage_ * dt);
            concrete->setEnergy(concrete->energy() + energy_gain_ * dt);
            concrete->noteDamageDealt(damage_ * dt);

            if (other->energy() <= 0.0f) {
                other->kill("predation");
                concrete->noteKill();
            }
        }
    }
}

const char* PredatorHuntCapability::name() const {
    return "predator_hunt";
}

}  // namespace neuro
