#pragma once

#include "core/types/Aliases.h"
#include "runtime/ProtagonistEcologyConfig.h"

#include <cstdint>
#include <memory>
#include <random>
#include <string_view>
#include <vector>

namespace neuro {
class IGenome;
class Agent;
class World2D;
}

namespace neuro::routes::protagonist {

class BaseLayer;

enum class SpeciesArchetype : std::uint8_t {
    Protagonist = 0,
    LargeHerbivore,
    SmallForager,
    ApexPredator,
    PackHunter,
    AmbushPredator,
    Scavenger,
    Omnivore,
    // Layer 3 (FINAL_BLUEPRINT.md decision D3): stable food-chain bottom.
    // Fast-breeding small prey eaten by virtually every predator.
    // Loneliness setpoint 0 (lives in colonies); damage_threshold tiny
    // (one bite kill).
    Rabbit,
};

std::string_view speciesArchetypeName(SpeciesArchetype archetype);
SpeciesArchetype parseSpeciesArchetype(std::string_view name);
bool isPredatorArchetype(SpeciesArchetype archetype);
bool isHerbivoreArchetype(SpeciesArchetype archetype);

// Non-owning view of a background species' genome population.
// Multiple parallel episodes can share the same view safely (read-only).
struct BackgroundSpecies {
    SpeciesArchetype archetype{SpeciesArchetype::LargeHerbivore};
    const std::vector<std::unique_ptr<IGenome>>* population{nullptr};
};

std::size_t protagonistSensoryDim(const ProtagonistEcologyConfig& config);
std::size_t backgroundSensoryDim(const ProtagonistEcologyConfig& config);
std::size_t backgroundOutputDim(SpeciesArchetype archetype);

std::unique_ptr<Agent> makeProtagonistAgent(AgentId id,
                                            const IGenome& genome,
                                            const ProtagonistEcologyConfig& config,
                                            std::mt19937& rng,
                                            const BaseLayer* base_layer = nullptr);

std::unique_ptr<Agent> makeBackgroundAgent(AgentId id,
                                           SpeciesArchetype archetype,
                                           const IGenome& genome,
                                           const ProtagonistEcologyConfig& config,
                                           std::mt19937& rng);

void seedBootstrapPopulation(World2D& world,
                             const ProtagonistEcologyConfig& config,
                             std::uint64_t& next_agent_id,
                             const std::vector<std::unique_ptr<IGenome>>* protagonist_population = nullptr);

void seedTrainingPopulation(World2D& world,
                            const ProtagonistEcologyConfig& config,
                            const std::vector<std::unique_ptr<IGenome>>& protagonist_population,
                            const std::vector<BackgroundSpecies>& background_species,
                            std::uint64_t& next_agent_id);

}  // namespace neuro::routes::protagonist
