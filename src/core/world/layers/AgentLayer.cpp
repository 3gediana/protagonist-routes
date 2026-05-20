#include "core/world/layers/AgentLayer.h"

#include "core/agent/Agent.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IWorld.h"

namespace neuro {

AgentLayer::AgentLayer(float death_threshold, float base_metabolism)
    : death_threshold_(death_threshold),
      base_metabolism_(base_metabolism) {}

const char* AgentLayer::layerName() const {
    return kName;
}

void AgentLayer::onAttach(IWorld&) {
}

void AgentLayer::tick(IWorld& world, float dt) {
    for (auto& agent : agents_) {
        if (!agent->alive()) {
            continue;
        }

        agent->beginTick();
        agent->step(world, dt);
        agent->incrementAge(dt);

        const float previous_energy = agent->energy();
        float metabolism = base_metabolism_;
        if (agent->infected()) {
            metabolism *= 2.0f;
        }
        const float next_energy = previous_energy - metabolism * dt;
        agent->setEnergy(next_energy);
        if (next_energy != previous_energy) {
            world.eventBus().publish(events::Event{events::EventType::AgentEnergyChanged,
                                                   events::AgentEnergyChanged{agent->id(), previous_energy, next_energy},
                                                   world.currentTick(),
                                                   world.currentTimeSeconds()});
        }

        if (agent->energy() <= death_threshold_) {
            agent->kill("energy_depleted");
            world.eventBus().publish(events::Event{events::EventType::AgentDied,
                                                   events::AgentDied{agent->id(), "energy_depleted"},
                                                   world.currentTick(),
                                                   world.currentTimeSeconds()});
        }
    }
}

int AgentLayer::tickOrder() const {
    return 100;
}

void AgentLayer::addAgent(std::unique_ptr<Agent> agent, IWorld& world) {
    world.eventBus().publish(events::Event{events::EventType::AgentBorn,
                                           events::AgentBorn{agent->id(), agent->genomeId(), agent->position()},
                                           world.currentTick(),
                                           world.currentTimeSeconds()});
    agents_.push_back(std::move(agent));
}

void AgentLayer::clear() {
    agents_.clear();
}

std::size_t AgentLayer::size() const {
    return agents_.size();
}

std::size_t AgentLayer::livingCount() const {
    std::size_t count = 0;
    for (const auto& agent : agents_) {
        count += agent->alive() ? 1u : 0u;
    }
    return count;
}

std::vector<Agent*> AgentLayer::agents() {
    std::vector<Agent*> result;
    result.reserve(agents_.size());
    for (auto& agent : agents_) {
        result.push_back(agent.get());
    }
    return result;
}

std::vector<const Agent*> AgentLayer::agents() const {
    std::vector<const Agent*> result;
    result.reserve(agents_.size());
    for (const auto& agent : agents_) {
        result.push_back(agent.get());
    }
    return result;
}

}  // namespace neuro
