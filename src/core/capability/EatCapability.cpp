#include "core/capability/EatCapability.h"

#include "core/agent/Agent.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/ResourceLayer.h"

namespace neuro {

EatCapability::EatCapability(float eat_radius)
    : eat_radius_(eat_radius) {}

std::size_t EatCapability::consumedOutputs() const {
    return 0;
}

void EatCapability::apply(IAgent& agent, IWorld& world, std::span<const float>, float) {
    auto* resources = world.getLayer<ResourceLayer>();
    if (resources == nullptr) {
        return;
    }

    const float previous_energy = agent.energy();
    const float gained = resources->tryConsume(agent.position(), eat_radius_, world);
    if (gained <= 0.0f) {
        return;
    }

    agent.setEnergy(previous_energy + gained);
    world.eventBus().publish(events::Event{events::EventType::AgentAteFood,
                                           events::AgentAteFood{agent.id(), agent.position(), gained},
                                           world.currentTick()});
    world.eventBus().publish(events::Event{events::EventType::AgentEnergyChanged,
                                           events::AgentEnergyChanged{agent.id(), previous_energy, agent.energy()},
                                           world.currentTick()});

    if (auto* concrete = dynamic_cast<Agent*>(&agent)) {
        concrete->noteFoodEaten(gained);
    }
}

const char* EatCapability::name() const {
    return "eat";
}

}  // namespace neuro
