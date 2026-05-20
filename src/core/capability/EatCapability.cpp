#include "core/capability/EatCapability.h"

#include "core/agent/Agent.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/ResourceLayer.h"
#include "core/world/layers/signal/SignalLayer.h"

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
    const auto consume = resources->tryConsumeTyped(agent.position(), eat_radius_, world);
    if (consume.amount <= 0.0f) {
        return;
    }

    float gained = consume.amount;
    auto* concrete = dynamic_cast<Agent*>(&agent);
    if (concrete != nullptr) {
        // dietPreference 0.0 = pure herbivore, 1.0 = pure carnivore.
        // Plant resource: high gain for herbivore (1.15), low for carnivore (0.85).
        // Meat resource: high gain for carnivore (1.15), low for herbivore (0.85).
        const float diet = concrete->dietPreference();
        const float type_match = consume.is_meat ? diet : (1.0f - diet);
        gained *= 0.85f + 0.30f * type_match;
    }

    agent.setEnergy(previous_energy + gained);
    world.eventBus().publish(events::Event{events::EventType::AgentAteFood,
                                           events::AgentAteFood{agent.id(), agent.position(), gained},
                                           world.currentTick(),
                                           world.currentTimeSeconds()});
    world.eventBus().publish(events::Event{events::EventType::AgentEnergyChanged,
                                           events::AgentEnergyChanged{agent.id(), previous_energy, agent.energy()},
                                           world.currentTick(),
                                           world.currentTimeSeconds()});

    if (concrete != nullptr) {
        if (consume.is_meat) {
            concrete->noteMeatFood(gained);
        } else {
            concrete->notePlantFood(gained);
        }
    }

    // Auto-emit signal at food location: signal field becomes a food map
    // This creates the communication closed loop: food → signal → other agents follow → food → reward
    auto* signal_layer = world.getLayer<SignalLayer>();
    if (signal_layer != nullptr) {
        (void)signal_layer;
    }
}

const char* EatCapability::name() const {
    return "eat";
}

}  // namespace neuro
