#pragma once

#include "core/interfaces/IEvolutionStrategy.h"

#include <random>

namespace neuro::routes::protagonist {

class ProtagonistGenome;

class ProtagonistEvolution final : public IEvolutionStrategy {
public:
    ProtagonistEvolution(double elite_ratio,
                         std::size_t tournament_size,
                         double weight_perturb_rate,
                         double weight_reset_rate,
                         double weight_perturb_sigma,
                         std::uint32_t seed);

    std::vector<std::unique_ptr<IGenome>> reproduce(
        const std::vector<EvaluatedIndividual>& evaluated,
        const std::vector<IGenome*>& current_genomes,
        const std::vector<std::pair<GenomeId, GenomeId>>* mating_pairs = nullptr) override;

    const char* name() const override;

private:
    ProtagonistGenome crossover(const ProtagonistGenome& a,
                                const ProtagonistGenome& b,
                                GenomeId child_id);
    void mutate(ProtagonistGenome& genome);
    const ProtagonistGenome* findGenome(const std::vector<IGenome*>& current_genomes,
                                        GenomeId id) const;

    double elite_ratio_;
    std::size_t tournament_size_;
    double weight_perturb_rate_;
    double weight_reset_rate_;
    double weight_perturb_sigma_;
    std::mt19937 rng_;
};

}  // namespace neuro::routes::protagonist
