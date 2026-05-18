#include "core/world/layers/DiseaseLayer.h"
#include "core/world/layers/AgentLayer.h"
#include "core/agent/Agent.h"
#include "core/interfaces/IWorld.h"

#include <cmath>
#include <random>

namespace neuro {

DiseaseLayer::DiseaseLayer(float infection_radius,
                           float infection_prob,
                           float infection_duration,
                           float death_prob,
                           float initial_infection_rate,
                           std::uint32_t seed)
    : infection_radius_(infection_radius),
      infection_prob_(infection_prob),
      infection_duration_(infection_duration),
      death_prob_(death_prob),
      initial_infection_rate_(initial_infection_rate),
      rng_(seed == 0 ? std::random_device{}() : seed) {}

void DiseaseLayer::onAttach(IWorld& world) {
    auto* agent_layer = world.getLayer<AgentLayer>();
    if (agent_layer == nullptr) return;

    std::bernoulli_distribution infect(initial_infection_rate_);
    for (auto* agent : agent_layer->agents()) {
        if (infect(rng_)) {
            agent->setInfected(true);
        }
    }
}

void DiseaseLayer::tick(IWorld& world, float dt) {
    auto* agent_layer = world.getLayer<AgentLayer>();
    if (agent_layer == nullptr) return;

    const auto& agents = agent_layer->agents();

    for (auto* agent : agents) {
        if (!agent->infected()) continue;

        agent->advanceInfection(dt);

        if (agent->infectionProgress() >= infection_duration_) {
            std::bernoulli_distribution dies(death_prob_);
            if (dies(rng_)) {
                agent->kill("disease");
            } else {
                agent->setInfected(false);
            }
            continue;
        }

        for (auto* other : agents) {
            if (other == agent || other->infected() || !other->alive()) continue;

            const float dx = agent->position().x - other->position().x;
            const float dy = agent->position().y - other->position().y;
            const float dist = std::sqrt(dx * dx + dy * dy);

            if (dist < infection_radius_) {
                std::bernoulli_distribution transmits(infection_prob_ * dt);
                if (transmits(rng_)) {
                    other->setInfected(true);
                }
            }
        }
    }
}

}  // namespace neuro
