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
// L5 #9: optional `config` arg adds the BackgroundMemoryWriteCapability
// interface dim when memory is enabled. Defaults to a nullptr-equivalent
// empty config branch (returns the bare archetype dim) so older call
// sites that haven't been updated still compile.
std::size_t backgroundOutputDim(SpeciesArchetype archetype, const ProtagonistEcologyConfig* config = nullptr);

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

// Layer 4 (FINAL_BLUEPRINT.md): tag founder agents with archetype +
// per-species max_age. Each founder gets a unique lineage_id seeded
// from a counter. Call this AFTER makeProtagonistAgent /
// makeBackgroundAgent and BEFORE addAgent.
void stampLifecycleAndLineage(Agent& agent,
                              SpeciesArchetype archetype,
                              const ProtagonistEcologyConfig& config,
                              std::uint64_t lineage_id,
                              std::mt19937& jitter_rng);

// Layer 4: build a child agent given two parent agents. PopulationLayer
// uses this in its SpawnCallback. Currently asexual: clones the
// better-fit parent's NEAT genome, applies small Gaussian weight noise,
// builds an Agent with the correct archetype's perceptions/capabilities,
// and stamps lifecycle metadata. Returns nullptr for archetypes that
// don't support in-world breeding (e.g. Protagonist - protag breeding
// happens cross-episode through ProtagonistEvolution).
std::unique_ptr<Agent> makeChildFromParents(const Agent& parent_a,
                                            const Agent& parent_b,
                                            AgentId new_agent_id,
                                            GenomeId new_genome_id,
                                            const ProtagonistEcologyConfig& config,
                                            std::mt19937& rng);

}  // namespace neuro::routes::protagonist
