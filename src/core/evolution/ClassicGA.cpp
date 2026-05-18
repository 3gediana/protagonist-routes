#include "core/evolution/ClassicGA.h"

#include "core/brain/fixed/FixedTopologyGenome.h"
#include "core/evolution/selection/EliteSelection.h"
#include "core/evolution/selection/TournamentSelection.h"

#include <algorithm>
#include <stdexcept>

namespace neuro {

ClassicGA::ClassicGA(double elite_ratio,
                     std::size_t tournament_size,
                     double weight_perturb_rate,
                     double weight_reset_rate,
                     double weight_perturb_sigma,
                     std::uint32_t seed)
    : elite_ratio_(elite_ratio),
      tournament_size_(tournament_size),
      weight_perturb_rate_(weight_perturb_rate),
      weight_reset_rate_(weight_reset_rate),
      weight_perturb_sigma_(weight_perturb_sigma),
      rng_(seed == 0 ? std::random_device{}() : seed) {}

std::vector<std::unique_ptr<IGenome>> ClassicGA::reproduce(
    const std::vector<EvaluatedIndividual>& evaluated,
    const std::vector<IGenome*>& current_genomes) {
    if (evaluated.empty() || current_genomes.empty()) {
        return {};
    }

    GenomeId max_id = kInvalidGenomeId;
    for (const IGenome* genome : current_genomes) {
        if (genome != nullptr && genome->id().value > max_id.value) {
            max_id = genome->id();
        }
    }

    EliteSelection elite_selector(elite_ratio_);
    TournamentSelection tournament_selector(tournament_size_, static_cast<std::uint32_t>(rng_()));

    std::vector<std::unique_ptr<IGenome>> next_generation;
    next_generation.reserve(evaluated.size());

    const auto elites = elite_selector.select(evaluated);
    for (const auto& elite : elites) {
        const auto* elite_genome = findGenome(current_genomes, elite.genome_id);
        if (elite_genome == nullptr) {
            throw std::runtime_error("Elite genome not found");
        }

        auto child = std::make_unique<FixedTopologyGenome>(*elite_genome);
        child->setId(GenomeId{++max_id.value});
        next_generation.push_back(std::move(child));
    }

    while (next_generation.size() < evaluated.size()) {
        const auto parent_a_eval = tournament_selector.selectOne(evaluated);
        const auto parent_b_eval = tournament_selector.selectOne(evaluated);
        const auto* parent_a = findGenome(current_genomes, parent_a_eval.genome_id);
        const auto* parent_b = findGenome(current_genomes, parent_b_eval.genome_id);
        if (parent_a == nullptr || parent_b == nullptr) {
            throw std::runtime_error("Tournament parent genome not found");
        }

        auto child = std::make_unique<FixedTopologyGenome>(
            crossover(*parent_a, *parent_b, GenomeId{++max_id.value}));
        mutate(*child);
        next_generation.push_back(std::move(child));
    }

    return next_generation;
}

const char* ClassicGA::name() const {
    return "classic_ga";
}

FixedTopologyGenome ClassicGA::crossover(const FixedTopologyGenome& a,
                                         const FixedTopologyGenome& b,
                                         GenomeId child_id) {
    if (a.inputDim() != b.inputDim()
        || a.hiddenDim() != b.hiddenDim()
        || a.outputDim() != b.outputDim()) {
        throw std::invalid_argument("ClassicGA crossover requires identical topology");
    }

    FixedTopologyGenome child(child_id, a.inputDim(), a.hiddenDim(), a.outputDim());
    std::bernoulli_distribution pick_a(0.5);

    auto blend = [&](const std::vector<float>& lhs, const std::vector<float>& rhs, std::vector<float>& out) {
        for (std::size_t i = 0; i < out.size(); ++i) {
            out[i] = pick_a(rng_) ? lhs[i] : rhs[i];
        }
    };

    blend(a.inputHiddenWeights(), b.inputHiddenWeights(), child.inputHiddenWeights());
    blend(a.hiddenBias(), b.hiddenBias(), child.hiddenBias());
    blend(a.hiddenOutputWeights(), b.hiddenOutputWeights(), child.hiddenOutputWeights());
    blend(a.outputBias(), b.outputBias(), child.outputBias());
    return child;
}

void ClassicGA::mutate(FixedTopologyGenome& genome) {
    std::bernoulli_distribution perturb(weight_perturb_rate_);
    std::bernoulli_distribution reset(weight_reset_rate_);
    std::normal_distribution<float> perturb_dist(0.0f, static_cast<float>(weight_perturb_sigma_));
    std::normal_distribution<float> reset_dist(0.0f, 1.0f);

    auto mutate_weights = [&](std::vector<float>& values) {
        for (float& value : values) {
            if (reset(rng_)) {
                value = reset_dist(rng_);
            } else if (perturb(rng_)) {
                value += perturb_dist(rng_);
            }
        }
    };

    mutate_weights(genome.inputHiddenWeights());
    mutate_weights(genome.hiddenBias());
    mutate_weights(genome.hiddenOutputWeights());
    mutate_weights(genome.outputBias());
}

const FixedTopologyGenome* ClassicGA::findGenome(const std::vector<IGenome*>& current_genomes,
                                                 GenomeId id) const {
    for (const IGenome* genome : current_genomes) {
        if (genome != nullptr && genome->id() == id) {
            return dynamic_cast<const FixedTopologyGenome*>(genome);
        }
    }
    return nullptr;
}

}  // namespace neuro
