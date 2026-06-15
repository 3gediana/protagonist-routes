#include "runtime/ProtagonistEvolution.h"

#include "brain/ProtagonistGenome.h"
#include "core/evolution/selection/EliteSelection.h"
#include "core/evolution/selection/TournamentSelection.h"

#include <sstream>
#include <stdexcept>

namespace neuro::routes::protagonist {

ProtagonistEvolution::ProtagonistEvolution(double elite_ratio,
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

std::vector<std::unique_ptr<IGenome>> ProtagonistEvolution::reproduce(
    const std::vector<EvaluatedIndividual>& evaluated,
    const std::vector<IGenome*>& current_genomes,
    const std::vector<std::pair<GenomeId, GenomeId>>* mating_pairs) {
    (void)mating_pairs;
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
            throw std::runtime_error("Elite protagonist genome not found");
        }

        auto child = std::make_unique<ProtagonistGenome>(*elite_genome);
        child->setId(GenomeId{++max_id.value});
        next_generation.push_back(std::move(child));
    }

    while (next_generation.size() < evaluated.size()) {
        const auto parent_a_eval = tournament_selector.selectOne(evaluated);
        const auto parent_b_eval = tournament_selector.selectOne(evaluated);
        const auto* parent_a = findGenome(current_genomes, parent_a_eval.genome_id);
        const auto* parent_b = findGenome(current_genomes, parent_b_eval.genome_id);
        if (parent_a == nullptr || parent_b == nullptr) {
            throw std::runtime_error("Tournament protagonist parent genome not found");
        }

        auto child = std::make_unique<ProtagonistGenome>(crossover(*parent_a, *parent_b, GenomeId{++max_id.value}));
        mutate(*child);
        next_generation.push_back(std::move(child));
    }

    return next_generation;
}

const char* ProtagonistEvolution::name() const {
    return "protagonist_ga";
}

ProtagonistEvolutionState ProtagonistEvolution::exportState() const {
    std::ostringstream rng_state;
    rng_state << rng_;
    return ProtagonistEvolutionState{rng_state.str()};
}

void ProtagonistEvolution::importState(const ProtagonistEvolutionState& state) {
    if (state.rng_state.empty()) {
        return;
    }
    std::istringstream rng_stream(state.rng_state);
    rng_stream >> rng_;
    if (!rng_stream) {
        throw std::runtime_error("ProtagonistEvolution: failed to restore RNG state");
    }
}

ProtagonistGenome ProtagonistEvolution::crossover(const ProtagonistGenome& a,
                                                  const ProtagonistGenome& b,
                                                  GenomeId child_id) {
    if (a.inputDim() != b.inputDim()
        || a.hiddenDim() != b.hiddenDim()
        || a.memoryCells() != b.memoryCells()
        || a.memorySlots() != b.memorySlots()
        || a.readHeads() != b.readHeads()
        || a.actionDim() != b.actionDim()) {
        throw std::invalid_argument("ProtagonistEvolution crossover requires identical topology");
    }

    ProtagonistGenome child(child_id, a.inputDim(), a.hiddenDim(), a.memoryCells(), a.memorySlots(), a.readHeads(), a.actionDim());
    std::bernoulli_distribution pick_a(0.5);

    auto blend = [&](const std::vector<float>& lhs, const std::vector<float>& rhs, std::vector<float>& out) {
        for (std::size_t i = 0; i < out.size(); ++i) {
            out[i] = pick_a(rng_) ? lhs[i] : rhs[i];
        }
    };

    blend(a.inputHiddenWeights(), b.inputHiddenWeights(), child.inputHiddenWeights());
    blend(a.hiddenBias(), b.hiddenBias(), child.hiddenBias());
    blend(a.hiddenActionWeights(), b.hiddenActionWeights(), child.hiddenActionWeights());
    blend(a.actionBias(), b.actionBias(), child.actionBias());
    blend(a.hiddenInterfaceWeights(), b.hiddenInterfaceWeights(), child.hiddenInterfaceWeights());
    blend(a.interfaceBias(), b.interfaceBias(), child.interfaceBias());
    return child;
}

void ProtagonistEvolution::mutate(ProtagonistGenome& genome) {
    mutateBoosted(genome, 1.0);
}

// L2.9 v15: same as mutate() but with weight_perturb_sigma scaled by
// sigma_multiplier. Used on the archive-breeding path so v15 can apply
// extra exploration noise without affecting v8-v13 reproduce path.
void ProtagonistEvolution::mutateBoosted(ProtagonistGenome& genome, double sigma_multiplier) {
    std::bernoulli_distribution perturb(weight_perturb_rate_);
    std::bernoulli_distribution reset(weight_reset_rate_);
    const float boosted_sigma = static_cast<float>(weight_perturb_sigma_ * sigma_multiplier);
    std::normal_distribution<float> perturb_dist(0.0f, boosted_sigma);
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
    mutate_weights(genome.hiddenActionWeights());
    mutate_weights(genome.actionBias());
    mutate_weights(genome.hiddenInterfaceWeights());
    mutate_weights(genome.interfaceBias());
}

// L2.9 v15: iso+line crossover (Vassiliades 2018 "Discovering the Elite
// Hypervolume"). child[i] = a[i] + alpha*(b[i]-a[i]) with a single
// alpha sampled per genome (not per weight) so the child sits ON the
// line between two elite parents, extending the elite hypervolume.
// Empirically gives 3-4x coverage vs uniform crossover in MAP-Elites.
ProtagonistGenome ProtagonistEvolution::crossoverLine(const ProtagonistGenome& a,
                                                     const ProtagonistGenome& b,
                                                     GenomeId child_id,
                                                     double line_sigma) {
    if (a.inputDim() != b.inputDim()
        || a.hiddenDim() != b.hiddenDim()
        || a.memoryCells() != b.memoryCells()
        || a.memorySlots() != b.memorySlots()
        || a.readHeads() != b.readHeads()
        || a.actionDim() != b.actionDim()) {
        throw std::invalid_argument("ProtagonistEvolution crossoverLine requires identical topology");
    }

    ProtagonistGenome child(child_id, a.inputDim(), a.hiddenDim(), a.memoryCells(), a.memorySlots(), a.readHeads(), a.actionDim());
    std::normal_distribution<float> alpha_dist(0.0f, static_cast<float>(line_sigma));
    const float alpha = alpha_dist(rng_);

    auto blend = [&](const std::vector<float>& lhs, const std::vector<float>& rhs, std::vector<float>& out) {
        for (std::size_t i = 0; i < out.size(); ++i) {
            out[i] = lhs[i] + alpha * (rhs[i] - lhs[i]);
        }
    };

    blend(a.inputHiddenWeights(), b.inputHiddenWeights(), child.inputHiddenWeights());
    blend(a.hiddenBias(), b.hiddenBias(), child.hiddenBias());
    blend(a.hiddenActionWeights(), b.hiddenActionWeights(), child.hiddenActionWeights());
    blend(a.actionBias(), b.actionBias(), child.actionBias());
    blend(a.hiddenInterfaceWeights(), b.hiddenInterfaceWeights(), child.hiddenInterfaceWeights());
    blend(a.interfaceBias(), b.interfaceBias(), child.interfaceBias());
    return child;
}

// L2.9 v15 "real" ME-NEAT reproduction. See header for behaviour.
std::vector<std::unique_ptr<IGenome>> ProtagonistEvolution::reproduceFromArchive(
    const std::vector<GenomeId>& archive_elite_ids,
    const std::vector<IGenome*>& current_genomes,
    std::size_t population_size,
    std::size_t current_generation,
    std::size_t init_generations,
    double line_sigma,
    double mutation_boost) {
    if (current_genomes.empty() || population_size == 0) {
        return {};
    }

    GenomeId max_id = kInvalidGenomeId;
    for (const IGenome* genome : current_genomes) {
        if (genome != nullptr && genome->id().value > max_id.value) {
            max_id = genome->id();
        }
    }

    std::vector<std::unique_ptr<IGenome>> next_generation;
    next_generation.reserve(population_size);

    const bool in_init_phase = current_generation < init_generations;

    // Phase 1: preserve cell elites (except during init phase, when
    // every offspring is a fresh random crossover so the archive can
    // fill from scratch).
    if (!in_init_phase) {
        for (GenomeId id : archive_elite_ids) {
            const auto* elite = findGenome(current_genomes, id);
            if (elite == nullptr) continue;
            auto child = std::make_unique<ProtagonistGenome>(*elite);
            child->setId(GenomeId{++max_id.value});
            next_generation.push_back(std::move(child));
            if (next_generation.size() >= population_size) {
                return next_generation;
            }
        }
    }

    // Phase 2: fill remaining slots by sampling parents (archive-uniform
    // if archive has >=2 entries, else random from current_genomes).
    const bool use_archive = !in_init_phase && archive_elite_ids.size() >= 2;
    std::uniform_int_distribution<std::size_t> archive_dist(
        0, archive_elite_ids.empty() ? 0 : archive_elite_ids.size() - 1);
    std::uniform_int_distribution<std::size_t> pop_dist(0, current_genomes.size() - 1);

    auto pick_parent = [&]() -> const ProtagonistGenome* {
        if (use_archive) {
            GenomeId id = archive_elite_ids[archive_dist(rng_)];
            return findGenome(current_genomes, id);
        }
        IGenome* g = current_genomes[pop_dist(rng_)];
        return g != nullptr ? dynamic_cast<const ProtagonistGenome*>(g) : nullptr;
    };

    while (next_generation.size() < population_size) {
        const ProtagonistGenome* parent_a = pick_parent();
        const ProtagonistGenome* parent_b = pick_parent();
        if (parent_a == nullptr || parent_b == nullptr) {
            // Defensive: if pick_parent failed (population had nulls),
            // skip this slot rather than crash. Worst case the next
            // population is short, which will be regrown next gen.
            break;
        }
        ProtagonistGenome child = (line_sigma > 0.0)
            ? crossoverLine(*parent_a, *parent_b, GenomeId{++max_id.value}, line_sigma)
            : crossover(*parent_a, *parent_b, GenomeId{++max_id.value});
        mutateBoosted(child, mutation_boost);
        next_generation.push_back(std::make_unique<ProtagonistGenome>(std::move(child)));
    }

    return next_generation;
}

std::vector<std::unique_ptr<IGenome>> ProtagonistEvolution::reproduceFromArchiveGenomes(
    const std::vector<const ProtagonistGenome*>& archive_elites,
    const std::vector<IGenome*>& current_genomes,
    std::size_t population_size,
    std::size_t current_generation,
    std::size_t init_generations,
    double line_sigma,
    double mutation_boost) {
    if (current_genomes.empty() || population_size == 0) {
        return {};
    }

    GenomeId max_id = kInvalidGenomeId;
    for (const IGenome* genome : current_genomes) {
        if (genome != nullptr && genome->id().value > max_id.value) {
            max_id = genome->id();
        }
    }

    std::vector<const ProtagonistGenome*> valid_archive_elites;
    valid_archive_elites.reserve(archive_elites.size());
    for (const ProtagonistGenome* elite : archive_elites) {
        if (elite != nullptr) {
            valid_archive_elites.push_back(elite);
        }
    }

    std::vector<std::unique_ptr<IGenome>> next_generation;
    next_generation.reserve(population_size);

    const bool in_init_phase = current_generation < init_generations;
    if (!in_init_phase) {
        for (const ProtagonistGenome* elite : valid_archive_elites) {
            auto child = std::make_unique<ProtagonistGenome>(*elite);
            child->setId(GenomeId{++max_id.value});
            next_generation.push_back(std::move(child));
            if (next_generation.size() >= population_size) {
                return next_generation;
            }
        }
    }

    const bool use_archive = !in_init_phase && valid_archive_elites.size() >= 2;
    std::uniform_int_distribution<std::size_t> archive_dist(
        0, valid_archive_elites.empty() ? 0 : valid_archive_elites.size() - 1);
    std::uniform_int_distribution<std::size_t> pop_dist(0, current_genomes.size() - 1);

    auto pick_parent = [&]() -> const ProtagonistGenome* {
        if (use_archive) {
            return valid_archive_elites[archive_dist(rng_)];
        }
        IGenome* g = current_genomes[pop_dist(rng_)];
        return g != nullptr ? dynamic_cast<const ProtagonistGenome*>(g) : nullptr;
    };

    while (next_generation.size() < population_size) {
        const ProtagonistGenome* parent_a = pick_parent();
        const ProtagonistGenome* parent_b = pick_parent();
        if (parent_a == nullptr || parent_b == nullptr) {
            break;
        }
        ProtagonistGenome child = (line_sigma > 0.0)
            ? crossoverLine(*parent_a, *parent_b, GenomeId{++max_id.value}, line_sigma)
            : crossover(*parent_a, *parent_b, GenomeId{++max_id.value});
        mutateBoosted(child, mutation_boost);
        next_generation.push_back(std::make_unique<ProtagonistGenome>(std::move(child)));
    }

    return next_generation;
}

const ProtagonistGenome* ProtagonistEvolution::findGenome(const std::vector<IGenome*>& current_genomes,
                                                          GenomeId id) const {
    for (const IGenome* genome : current_genomes) {
        if (genome != nullptr && genome->id() == id) {
            return dynamic_cast<const ProtagonistGenome*>(genome);
        }
    }
    return nullptr;
}

}  // namespace neuro::routes::protagonist
