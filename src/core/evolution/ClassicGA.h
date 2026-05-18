#pragma once

#include "core/interfaces/IEvolutionStrategy.h"

#include <random>

namespace neuro {

class FixedTopologyGenome;
class EliteSelection;
class TournamentSelection;

class ClassicGA final : public IEvolutionStrategy {
public:
    ClassicGA(double elite_ratio,
              std::size_t tournament_size,
              double weight_perturb_rate,
              double weight_reset_rate,
              double weight_perturb_sigma,
              std::uint32_t seed);

    std::vector<std::unique_ptr<IGenome>> reproduce(
        const std::vector<EvaluatedIndividual>& evaluated,
        const std::vector<IGenome*>& current_genomes) override;

    const char* name() const override;

private:
    FixedTopologyGenome crossover(const FixedTopologyGenome& a,
                                  const FixedTopologyGenome& b,
                                  GenomeId child_id);
    void mutate(FixedTopologyGenome& genome);
    const FixedTopologyGenome* findGenome(const std::vector<IGenome*>& current_genomes,
                                          GenomeId id) const;

    double elite_ratio_;
    std::size_t tournament_size_;
    double weight_perturb_rate_;
    double weight_reset_rate_;
    double weight_perturb_sigma_;
    std::mt19937 rng_;
};

}  // namespace neuro
