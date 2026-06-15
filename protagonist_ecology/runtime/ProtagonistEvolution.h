#pragma once

#include "core/interfaces/IEvolutionStrategy.h"

#include <random>
#include <string>

namespace neuro::routes::protagonist {

class ProtagonistGenome;

struct ProtagonistEvolutionState {
    std::string rng_state;
};

// P0 self-adaptive mutation (Schwefel/Beyer self-adaptive ES). When enabled,
// each genome carries its own mutation step-size sigma which is itself mutated
// log-normally (sigma' = sigma * exp(tau * N(0,1))) and clamped to
// [sigma_min, sigma_max]; offspring inherit the parents' averaged sigma. This
// removes the externally-tuned weight_perturb_sigma / line_sigma /
// mutation_boost / weight_perturb_rate / weight_reset_rate knobs. When
// disabled (default) the legacy fixed-sigma path runs unchanged.
struct SelfAdaptiveSigma {
    bool enabled = false;
    double initial_sigma = 0.10;
    double learning_rate = 0.05;  // tau in sigma' = sigma * exp(tau * N(0,1))
    double sigma_min = 0.001;
    double sigma_max = 2.0;
};

class ProtagonistEvolution final : public IEvolutionStrategy {
public:
    ProtagonistEvolution(double elite_ratio,
                         std::size_t tournament_size,
                         double weight_perturb_rate,
                         double weight_reset_rate,
                         double weight_perturb_sigma,
                         std::uint32_t seed,
                         SelfAdaptiveSigma self_adaptive = {});

    std::vector<std::unique_ptr<IGenome>> reproduce(
        const std::vector<EvaluatedIndividual>& evaluated,
        const std::vector<IGenome*>& current_genomes,
        const std::vector<std::pair<GenomeId, GenomeId>>* mating_pairs = nullptr) override;

    // L2.9 v15 "real" ME-NEAT path. Replaces EliteSelection + Tournament
    // with archive-uniform parent sampling (Mouret 2015 Tutorial step
    // "Pick two elites from the Archive"). Behaviour:
    //
    //   - Each cell elite from archive_elite_ids is preserved as-is in
    //     the next generation (clone, new id).
    //   - Remaining offspring slots are filled by:
    //       a) if archive has >=2 cells: pick two parents uniformly
    //          from archive_elite_ids, crossover (iso+line if
    //          line_sigma>0, else uniform), mutate (sigma *= mutation_boost).
    //       b) if archive has 0/1 cells (early gens): fall back to two
    //          random parents from current_genomes to seed diversity.
    //   - In the first init_generations, even archive-elite preservation
    //     is skipped (all offspring are random crossovers) so the
    //     archive can fill from scratch (Nilsson 2021).
    //
    // population_size targets evaluated.size() to match the existing
    // contract.
    std::vector<std::unique_ptr<IGenome>> reproduceFromArchive(
        const std::vector<GenomeId>& archive_elite_ids,
        const std::vector<IGenome*>& current_genomes,
        std::size_t population_size,
        std::size_t current_generation,
        std::size_t init_generations,
        double line_sigma,
        double mutation_boost);

    std::vector<std::unique_ptr<IGenome>> reproduceFromArchiveGenomes(
        const std::vector<const ProtagonistGenome*>& archive_elites,
        const std::vector<IGenome*>& current_genomes,
        std::size_t population_size,
        std::size_t current_generation,
        std::size_t init_generations,
        double line_sigma,
        double mutation_boost);

    const char* name() const override;

    ProtagonistEvolutionState exportState() const;
    void importState(const ProtagonistEvolutionState& state);

private:
    ProtagonistGenome crossover(const ProtagonistGenome& a,
                                const ProtagonistGenome& b,
                                GenomeId child_id);
    // iso+line crossover (Vassiliades 2018): child[i] = a[i] + alpha*(b[i]-a[i])
    // with alpha ~ N(0, line_sigma). The "iso" part (a[i] + N(0,sigma_iso))
    // comes for free from the subsequent mutate() call.
    ProtagonistGenome crossoverLine(const ProtagonistGenome& a,
                                    const ProtagonistGenome& b,
                                    GenomeId child_id,
                                    double line_sigma);
    void mutate(ProtagonistGenome& genome);
    void mutateBoosted(ProtagonistGenome& genome, double sigma_multiplier);
    const ProtagonistGenome* findGenome(const std::vector<IGenome*>& current_genomes,
                                        GenomeId id) const;

    double elite_ratio_;
    std::size_t tournament_size_;
    double weight_perturb_rate_;
    double weight_reset_rate_;
    double weight_perturb_sigma_;
    SelfAdaptiveSigma self_adaptive_;
    std::mt19937 rng_;

    // Self-adapt a single genome's sigma in place (log-normal) and clamp it.
    float adaptSigma(float sigma);
};

}  // namespace neuro::routes::protagonist
