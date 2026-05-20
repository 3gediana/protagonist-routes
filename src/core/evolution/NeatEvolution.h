#pragma once

#include "core/brain/neat/InnovationTracker.h"
#include "core/evolution/speciation/Speciation.h"
#include "core/interfaces/IEvolutionStrategy.h"

#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace neuro {

class NeatGenome;

struct NeatEvolutionState {
    std::string rng_state;
    SpeciationState speciation_state;
};

class NeatEvolution final : public IEvolutionStrategy {
public:
    NeatEvolution(double elite_ratio,
                  std::size_t tournament_size,
                  double weight_perturb_rate,
                  double weight_reset_rate,
                  double add_node_rate,
                  double add_connection_rate,
                  double toggle_connection_rate,
                  double add_modulatory_rate,
                  double weight_perturb_sigma,
                  double compatibility_threshold,
                  double c1_excess,
                  double c2_disjoint,
                  double c3_weight,
                  float tau_min,
                  float tau_max,
                  std::uint32_t seed,
                  float decision_interval_min = 1.0f,
                  float decision_interval_max = 30.0f);

    std::vector<std::unique_ptr<IGenome>> reproduce(
        const std::vector<EvaluatedIndividual>& evaluated,
        const std::vector<IGenome*>& current_genomes,
        const std::vector<std::pair<GenomeId, GenomeId>>* mating_pairs = nullptr) override;

    const char* name() const override;

    InnovationTracker& innovationTracker();
    std::vector<SpeciesGroup> assignSpecies(const std::vector<IGenome*>& current_genomes);
    const std::vector<SpeciesGroup>& lastSpeciesGroups() const;
    NeatEvolutionState exportState() const;
    void importState(const NeatEvolutionState& state);

private:
    struct SpeciesState {
        SpeciesGroup group;
        std::vector<EvaluatedIndividual> members;
        double adjusted_total_fitness{0.0};
        std::size_t spawn_count{0};
    };

    const NeatGenome* findGenome(const std::vector<IGenome*>& current_genomes, GenomeId id) const;
    std::vector<SpeciesState> buildSpeciesStates(const std::vector<EvaluatedIndividual>& evaluated,
                                                 const std::vector<IGenome*>& current_genomes);
    std::vector<std::size_t> allocateSpawnCounts(const std::vector<SpeciesState>& species,
                                                 std::size_t population_size) const;
    NeatGenome crossover(const NeatGenome& a,
                         const NeatGenome& b,
                         const EvaluatedIndividual& a_eval,
                         const EvaluatedIndividual& b_eval,
                         GenomeId child_id);
    void mutate(NeatGenome& genome);
    void mutateWeights(NeatGenome& genome);
    void mutateNodeParameters(NeatGenome& genome);
    void addConnectionMutation(NeatGenome& genome);
    void addNodeMutation(NeatGenome& genome);
    void toggleConnectionMutation(NeatGenome& genome);
    void addModulatoryNodeMutation(NeatGenome& genome);
    void mutateDecisionInterval(NeatGenome& genome);
    void mutateImmunityGainRate(NeatGenome& genome);
    void mutateBehavioralGenes(NeatGenome& genome);

    double elite_ratio_;
    std::size_t tournament_size_;
    double weight_perturb_rate_;
    double weight_reset_rate_;
    double add_node_rate_;
    double add_connection_rate_;
    double toggle_connection_rate_;
    double add_modulatory_rate_;
    double weight_perturb_sigma_;
    float tau_min_;
    float tau_max_;
    std::size_t target_species_min_;
    std::size_t target_species_max_;
    double compatibility_adjustment_step_;
    float decision_interval_min_;
    float decision_interval_max_;
    std::mt19937 rng_;
    InnovationTracker innovation_tracker_;
    Speciation speciation_;
    std::vector<SpeciesGroup> last_species_groups_;
};

}  // namespace neuro
