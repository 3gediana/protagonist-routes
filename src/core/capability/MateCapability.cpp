#include "core/capability/MateCapability.h"

#include "core/agent/Agent.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"
#include "core/events/EventBus.h"
#include "core/events/EventType.h"

#include <cmath>

namespace neuro {

MateCapability::MateCapability(float mate_radius, float energy_cost)
    : mate_radius_(mate_radius), energy_cost_(energy_cost) {}

void MateCapability::apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) {
    (void)dt;
    if (outputs.empty() || outputs[0] < 0.5f) return;

    auto* concrete = dynamic_cast<Agent*>(&agent);
    if (!concrete || !concrete->alive()) return;

    // Need enough energy to mate
    if (concrete->energy() < energy_cost_ * 2.0f) return;

    auto* agent_layer = world.getLayer<AgentLayer>();
    if (!agent_layer) return;

    // Find nearest same-species agent within mate_radius
    Agent* partner = nullptr;
    float best_dist = mate_radius_;

    for (Agent* other : agent_layer->agents()) {
        if (other == concrete || !other->alive()) continue;
        if (other->isPredator() != concrete->isPredator()) continue;
        if (other->speciesId() != concrete->speciesId()) continue;

        const float dx = agent.position().x - other->position().x;
        const float dy = agent.position().y - other->position().y;
        const float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < best_dist) {
            best_dist = dist;
            partner = other;
        }
    }

    if (!partner) return;

    // Both agents pay energy cost
    concrete->setEnergy(concrete->energy() - energy_cost_);
    partner->setEnergy(partner->energy() - energy_cost_);

    // Record mating event for evolution system
    // Payload: pair of agent IDs (initiator, partner)
    world.eventBus().publish(events::EventType::MatingOccurred,
        events::MatingOccurred{agent.id(), partner->id()},
        world.currentTick());
}

}  // namespace neuro
