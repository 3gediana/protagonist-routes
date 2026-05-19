#pragma once
//
// core/interfaces/IEvolutionStrategy.h
//
// Drives one generation of evolution: select parents from the evaluated
// population, produce offspring, mutate them, and return the next
// generation's genomes.
//
// Implementations:
//   M1: ClassicGA              - elitism + tournament + uniform crossover
//   M3: NeatEvolution          - speciation + fitness sharing
//   F-NSGA: NsgaIIEvolution    - Pareto fronts
//
// The strategy is invoked between generations by HeadlessRuntime; it
// has no opinion about how the world evaluated fitness, only about how
// to select for the next.

#include "core/types/Aliases.h"

#include <memory>
#include <vector>

namespace neuro {

class IGenome;
class IAgent;

// One entry summarizing an agent's outcome at the end of a generation.
struct EvaluatedIndividual {
    GenomeId  genome_id;
    double    fitness;
    SpeciesId species_id;        // kInvalidSpeciesId pre-M3
    std::size_t complexity;      // genome complexity (for stats)
    std::size_t modulatory_nodes; // count of modulatory nodes in genome
    float     immunity_gain_rate; // evolvable disease resistance trait
};

class IEvolutionStrategy {
public:
    virtual ~IEvolutionStrategy() = default;

    // Produce next generation's genomes given evaluation results and the
    // current pool of genomes. Returned vector size should match the
    // population size declared in config (the strategy enforces this).
    //
    // The strategy may consult `current_genomes` to look up actual genome
    // contents by id. The returned new genomes are NEW instances (clones
    // of selected parents, possibly mutated).
    virtual std::vector<std::unique_ptr<IGenome>> reproduce(
        const std::vector<EvaluatedIndividual>& evaluated,
        const std::vector<IGenome*>& current_genomes) = 0;

    // Strategy name (for logging).
    virtual const char* name() const = 0;
};

}  // namespace neuro
