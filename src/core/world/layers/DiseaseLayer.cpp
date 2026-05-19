#include "core/world/layers/DiseaseLayer.h"
#include "core/world/layers/AgentLayer.h"
#include "core/world/layers/SeasonLayer.h"
#include "core/world/layers/terrain/TerrainLayer.h"
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
    (void)world;
}

void DiseaseLayer::tick(IWorld& world, float dt) {
    auto* agent_layer = world.getLayer<AgentLayer>();
    if (agent_layer == nullptr) return;

    if (infection_deaths_ == 0 && recoveries_ == 0 && agent_layer->size() > 0) {
        std::bernoulli_distribution infect(initial_infection_rate_);
        for (auto* agent : agent_layer->agents()) {
            if (infect(rng_)) {
                agent->setInfected(true);
            }
        }
    }

    const auto& agents = agent_layer->agents();

    float season_factor = 1.0f;
    const auto* season = world.getLayer<SeasonLayer>();
    if (season) {
        switch (season->currentSeason()) {
            case SeasonType::Spring: season_factor = 1.0f; break;
            case SeasonType::Summer: season_factor = 0.5f; break;
            case SeasonType::Autumn: season_factor = 1.5f; break;
            case SeasonType::Winter: season_factor = 2.5f; break;
        }
    }

    const auto* terrain = world.getLayer<TerrainLayer>();

    for (auto* agent : agents) {
        if (!agent->infected()) continue;

        // Immunity race: both infection and immunity progress simultaneously
        // Well-fed agents gain immunity faster (energy > 50 = bonus)
        agent->advanceInfection(dt);
        const float energy_factor = 1.0f + std::max(0.0f, agent->energy() - 50.0f) / 100.0f;
        agent->advanceImmunity(dt * energy_factor);

        // Immunity wins → recover + gain immune status
        if (agent->immunityProgress() >= 1.0f) {
            agent->setInfected(false);
            agent->setImmune(true);
            ++recoveries_;
            continue;
        }

        // Infection wins → die
        if (agent->infectionProgress() >= infection_duration_) {
            agent->kill("disease");
            ++infection_deaths_;
            continue;
        }

        // Spread to nearby healthy non-immune agents
        for (auto* other : agents) {
            if (other == agent || other->infected() || !other->alive()) continue;
            if (other->immune()) continue;

            const float dx = agent->position().x - other->position().x;
            const float dy = agent->position().y - other->position().y;
            const float dist = std::sqrt(dx * dx + dy * dy);

            if (dist < infection_radius_) {
                float shelter_factor = 1.0f;
                if (terrain && terrain->inCave(other->position().x, other->position().y)) {
                    shelter_factor = 0.3f;
                }
                const float effective_prob = infection_prob_ * season_factor * shelter_factor * dt;
                std::bernoulli_distribution transmits(effective_prob);
                if (transmits(rng_)) {
                    other->setInfected(true);
                }
            }
        }
    }
}

}  // namespace neuro
