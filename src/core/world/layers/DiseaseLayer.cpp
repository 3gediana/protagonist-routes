#include "core/world/layers/DiseaseLayer.h"
#include "core/world/layers/AgentLayer.h"
#include "core/world/layers/SeasonLayer.h"
#include "core/world/layers/terrain/TerrainLayer.h"
#include "core/agent/Agent.h"
#include "core/pathogen/Pathogen.h"
#include "core/interfaces/IWorld.h"

#include <algorithm>
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

    // Initial infection: seed some pathogens
    if (infection_deaths_ == 0 && recoveries_ == 0 && agent_layer->size() > 0) {
        std::bernoulli_distribution infect(initial_infection_rate_);
        for (auto* agent : agent_layer->agents()) {
            if (infect(rng_)) {
                infectAgent(agent);
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

        auto* pathogen = agent->pathogen();
        if (pathogen == nullptr) {
            // Legacy: infected without pathogen, create one
            infectAgent(agent);
            pathogen = agent->pathogen();
            if (pathogen == nullptr) continue;
        }

        // Pathogen "thinks" based on host state
        const float energy_norm = std::clamp(agent->energy() / 100.0f, 0.0f, 1.0f);
        const float infection_norm = std::clamp(agent->infectionProgress() / infection_duration_, 0.0f, 1.0f);
        const float immunity_norm = std::clamp(agent->immunityProgress(), 0.0f, 1.0f);
        pathogen->think(energy_norm, immunity_norm, infection_norm);

        // Infection advances at pathogen-determined speed
        agent->advanceInfection(dt * pathogen->infectionSpeed());

        // Immunity advances, reduced by pathogen's immune evasion
        const float effective_immunity_rate = agent->immunityGainRate() * (1.0f - pathogen->immuneEvasion());
        const float energy_factor = 1.0f + std::max(0.0f, agent->energy() - 50.0f) / 100.0f;
        agent->advanceImmunity(dt * effective_immunity_rate * energy_factor);

        // Immunity wins → recover + add strain to immune memory
        if (agent->immunityProgress() >= 1.0f) {
            const auto strain = pathogen->genome().strain();
            agent->setInfected(false);
            agent->addImmuneMemory(strain);
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

        // Spread to nearby agents that aren't immune to this strain
        // Selection pressure: pathogens that transmitted more get a spread boost
        const float fitness_boost = 1.0f + 0.1f * std::min(pathogen->genome().transmissionCount(), std::size_t(10));
        for (auto* other : agents) {
            if (other == agent || other->infected() || !other->alive()) continue;

            const float dx = agent->position().x - other->position().x;
            const float dy = agent->position().y - other->position().y;
            const float dist = std::sqrt(dx * dx + dy * dy);

            if (dist < infection_radius_) {
                float shelter_factor = 1.0f;
                if (terrain && terrain->inCave(other->position().x, other->position().y)) {
                    shelter_factor = 0.3f;
                }
                // Pathogen controls spread rate, boosted by transmission fitness
                const float effective_prob = infection_prob_ * pathogen->spreadRate()
                                           * fitness_boost * season_factor * shelter_factor * dt;
                std::bernoulli_distribution transmits(std::clamp(effective_prob, 0.0f, 1.0f));
                if (transmits(rng_)) {
                    // Pathogen reproduces: create mutated offspring (antigenic drift)
                    auto child = pathogen->offspring(static_cast<std::uint32_t>(rng_()));
                    pathogen->noteTransmission();

                    // Check if target has immunity to this specific strain
                    const auto* other_concrete = dynamic_cast<const Agent*>(other);
                    if (other_concrete && other_concrete->hasImmunityToStrain(child.genome().strain())) {
                        // Host immune to this strain — infection blocked
                        continue;
                    }

                    other->setInfected(true);
                    other->setImmune(false);
                    other->attachPathogen(std::make_unique<Pathogen>(std::move(child)));
                }
            }
        }
    }
}

void DiseaseLayer::infectAgent(Agent* agent) {
    agent->setInfected(true);
    agent->setImmune(false);
    PathogenGenome genome(static_cast<std::uint32_t>(rng_()));
    agent->attachPathogen(std::make_unique<Pathogen>(std::move(genome)));
}

}  // namespace neuro
