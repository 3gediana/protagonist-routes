#include "core/evolution/Nsga2Evolution.h"
#include "core/brain/neat/NeatGenome.h"
#include "core/brain/neat/NeatBrain.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>

namespace neuro {

Nsga2Evolution::Nsga2Evolution(
    double elite_ratio, std::size_t tournament_size,
    double weight_perturb_rate, double weight_reset_rate,
    double add_node_rate, double add_connection_rate,
    double toggle_connection_rate, double add_modulatory_rate,
    double weight_perturb_sigma,
    double compatibility_threshold,
    double c1_excess, double c2_disjoint, double c3_weight,
    float tau_min, float tau_max,
    std::uint32_t seed,
    float decision_interval_min, float decision_interval_max)
    : elite_ratio_(elite_ratio), tournament_size_(tournament_size),
      weight_perturb_rate_(weight_perturb_rate), weight_reset_rate_(weight_reset_rate),
      add_node_rate_(add_node_rate), add_connection_rate_(add_connection_rate),
      toggle_connection_rate_(toggle_connection_rate), add_modulatory_rate_(add_modulatory_rate),
      weight_perturb_sigma_(weight_perturb_sigma),
      tau_min_(tau_min), tau_max_(tau_max),
      decision_interval_min_(decision_interval_min), decision_interval_max_(decision_interval_max),
      rng_(seed == 0 ? std::random_device{}() : seed),
      speciation_(CompatibilityDistance(c1_excess, c2_disjoint, c3_weight), compatibility_threshold) {}

const char* Nsga2Evolution::name() const { return "nsga2"; }

InnovationTracker& Nsga2Evolution::innovationTracker() { return innovation_tracker_; }

std::vector<SpeciesGroup> Nsga2Evolution::assignSpecies(const std::vector<IGenome*>& current_genomes) {
    std::vector<const NeatGenome*> neat_genomes;
    neat_genomes.reserve(current_genomes.size());
    for (const IGenome* genome : current_genomes) {
        const auto* neat_genome = dynamic_cast<const NeatGenome*>(genome);
        if (neat_genome == nullptr) {
            throw std::runtime_error("Nsga2Evolution received non-NEAT genome");
        }
        neat_genomes.push_back(neat_genome);
    }
    last_species_groups_ = speciation_.assign(neat_genomes);
    return last_species_groups_;
}

const std::vector<SpeciesGroup>& Nsga2Evolution::lastSpeciesGroups() const {
    return last_species_groups_;
}

// --- NSGA-II non-dominated sort ---

std::vector<Nsga2Evolution::FrontAssignment> Nsga2Evolution::nonDominatedSort(
    const std::vector<EvaluatedIndividual>& evaluated) {

    const std::size_t n = evaluated.size();
    std::vector<FrontAssignment> result(n);
    std::vector<int> domination_count(n, 0);
    std::vector<std::vector<std::size_t>> dominated_by(n);

    std::size_t obj_dim = 0;
    for (const auto& e : evaluated) {
        if (!e.objectives.empty()) { obj_dim = e.objectives.size(); break; }
    }

    if (obj_dim == 0) {
        std::vector<std::size_t> indices(n);
        for (std::size_t i = 0; i < n; ++i) indices[i] = i;
        std::sort(indices.begin(), indices.end(), [&](std::size_t a, std::size_t b) {
            return evaluated[a].fitness > evaluated[b].fitness;
        });
        for (std::size_t i = 0; i < n; ++i) {
            result[indices[i]].rank = static_cast<int>(i);
            result[indices[i]].crowding = 0.0;
        }
        return result;
    }

    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = i + 1; j < n; ++j) {
            bool i_dominates_j = true;
            bool j_dominates_i = true;
            for (std::size_t k = 0; k < obj_dim; ++k) {
                if (evaluated[i].objectives[k] < evaluated[j].objectives[k]) i_dominates_j = false;
                if (evaluated[j].objectives[k] < evaluated[i].objectives[k]) j_dominates_i = false;
            }
            if (i_dominates_j && !j_dominates_i) {
                dominated_by[i].push_back(j);
                domination_count[j]++;
            } else if (j_dominates_i && !i_dominates_j) {
                dominated_by[j].push_back(i);
                domination_count[i]++;
            }
        }
    }

    std::vector<std::size_t> current_front;
    for (std::size_t i = 0; i < n; ++i) {
        if (domination_count[i] == 0) {
            current_front.push_back(i);
            result[i].rank = 0;
        }
    }

    int front_rank = 0;
    while (!current_front.empty()) {
        std::vector<std::size_t> next_front;
        for (auto idx : current_front) {
            for (auto dominated : dominated_by[idx]) {
                domination_count[dominated]--;
                if (domination_count[dominated] == 0) {
                    next_front.push_back(dominated);
                    result[dominated].rank = front_rank + 1;
                }
            }
        }
        auto crowding = computeCrowdingDistance(current_front, evaluated);
        for (std::size_t i = 0; i < current_front.size(); ++i) {
            result[current_front[i]].crowding = crowding[i];
        }
        current_front = next_front;
        front_rank++;
    }

    return result;
}

std::vector<double> Nsga2Evolution::computeCrowdingDistance(
    const std::vector<std::size_t>& front_indices,
    const std::vector<EvaluatedIndividual>& evaluated) {

    const std::size_t n = front_indices.size();
    if (n <= 2) {
        return std::vector<double>(n, std::numeric_limits<double>::infinity());
    }

    std::vector<double> crowding(n, 0.0);
    std::size_t obj_dim = evaluated[front_indices[0]].objectives.size();
    if (obj_dim == 0) obj_dim = 1;

    for (std::size_t obj = 0; obj < obj_dim; ++obj) {
        std::vector<std::size_t> sorted = front_indices;
        std::sort(sorted.begin(), sorted.end(), [&](std::size_t a, std::size_t b) {
            double va = obj < evaluated[a].objectives.size() ? evaluated[a].objectives[obj] : evaluated[a].fitness;
            double vb = obj < evaluated[b].objectives.size() ? evaluated[b].objectives[obj] : evaluated[b].fitness;
            return va < vb;
        });

        double obj_min = std::numeric_limits<double>::max();
        double obj_max = std::numeric_limits<double>::lowest();
        for (auto idx : front_indices) {
            double v = obj < evaluated[idx].objectives.size() ? evaluated[idx].objectives[obj] : evaluated[idx].fitness;
            obj_min = std::min(obj_min, v);
            obj_max = std::max(obj_max, v);
        }
        double range = obj_max - obj_min;
        if (range < 1e-12) range = 1.0;

        for (std::size_t i = 0; i < n; ++i) {
            if (front_indices[i] == sorted.front() || front_indices[i] == sorted.back()) {
                crowding[i] = std::numeric_limits<double>::infinity();
            }
        }

        for (std::size_t j = 1; j < sorted.size() - 1; ++j) {
            double v_next = obj < evaluated[sorted[j+1]].objectives.size() ? evaluated[sorted[j+1]].objectives[obj] : evaluated[sorted[j+1]].fitness;
            double v_prev = obj < evaluated[sorted[j-1]].objectives.size() ? evaluated[sorted[j-1]].objectives[obj] : evaluated[sorted[j-1]].fitness;
            double dist = (v_next - v_prev) / range;
            for (std::size_t i = 0; i < n; ++i) {
                if (front_indices[i] == sorted[j]) {
                    crowding[i] += dist;
                    break;
                }
            }
        }
    }

    return crowding;
}

// --- Reproduce using NSGA-II selection ---
std::vector<std::unique_ptr<IGenome>> Nsga2Evolution::reproduce(
    const std::vector<EvaluatedIndividual>& evaluated,
    const std::vector<IGenome*>& current_genomes,
    const std::vector<std::pair<GenomeId, GenomeId>>* mating_pairs) {

    if (evaluated.empty() || current_genomes.empty()) return {};

    const std::size_t target_size = current_genomes.size();
    GenomeId max_id = kInvalidGenomeId;
    for (const IGenome* genome : current_genomes) {
        if (genome != nullptr && genome->id().value > max_id.value) {
            max_id = genome->id();
        }
    }

    std::unordered_multimap<GenomeId, GenomeId> mate_lookup;
    if (mating_pairs != nullptr) {
        for (const auto& [lhs, rhs] : *mating_pairs) {
            if (!lhs || !rhs || lhs == rhs) {
                continue;
            }
            mate_lookup.emplace(lhs, rhs);
            mate_lookup.emplace(rhs, lhs);
        }
    }

    auto fronts = nonDominatedSort(evaluated);

    std::vector<std::size_t> sorted_indices(evaluated.size());
    for (std::size_t i = 0; i < evaluated.size(); ++i) sorted_indices[i] = i;
    std::sort(sorted_indices.begin(), sorted_indices.end(), [&](std::size_t a, std::size_t b) {
        if (fronts[a].rank != fronts[b].rank) return fronts[a].rank < fronts[b].rank;
        return fronts[a].crowding > fronts[b].crowding;
    });

    const std::size_t elite_count = std::max<std::size_t>(1,
        static_cast<std::size_t>(static_cast<double>(target_size) * elite_ratio_));

    std::vector<std::unique_ptr<IGenome>> next_gen;
    next_gen.reserve(target_size);

    for (std::size_t i = 0; i < elite_count && i < sorted_indices.size(); ++i) {
        const auto& eval = evaluated[sorted_indices[i]];
        const auto* genome = findGenome(current_genomes, eval.genome_id);
        if (genome) {
            auto child = genome->clone();
            if (auto* neat = dynamic_cast<NeatGenome*>(child.get())) {
                neat->setId(GenomeId{++max_id.value});
            }
            next_gen.push_back(std::move(child));
        }
    }

    std::uniform_int_distribution<std::size_t> idx_dist(0, evaluated.size() - 1);

    while (next_gen.size() < target_size) {
        auto nsga2_tournament = [&](std::size_t candidate) -> std::size_t {
            std::size_t best = candidate;
            for (std::size_t t = 1; t < tournament_size_; ++t) {
                std::size_t challenger = idx_dist(rng_);
                if (fronts[challenger].rank < fronts[best].rank ||
                    (fronts[challenger].rank == fronts[best].rank &&
                     fronts[challenger].crowding > fronts[best].crowding)) {
                    best = challenger;
                }
            }
            return best;
        };

        auto select_partner_idx = [&](std::size_t primary_idx) -> std::size_t {
            if (mate_lookup.empty() || !std::bernoulli_distribution(0.75)(rng_)) {
                return nsga2_tournament(idx_dist(rng_));
            }

            std::vector<std::size_t> candidates;
            const auto range = mate_lookup.equal_range(evaluated[primary_idx].genome_id);
            for (auto it = range.first; it != range.second; ++it) {
                for (std::size_t idx = 0; idx < evaluated.size(); ++idx) {
                    if (idx != primary_idx && evaluated[idx].genome_id == it->second) {
                        candidates.push_back(idx);
                    }
                }
            }

            if (candidates.empty()) {
                return nsga2_tournament(idx_dist(rng_));
            }

            std::uniform_int_distribution<std::size_t> candidate_dist(0, candidates.size() - 1);
            std::size_t best = candidates[candidate_dist(rng_)];
            for (std::size_t t = 1; t < tournament_size_; ++t) {
                std::size_t challenger = candidates[candidate_dist(rng_)];
                if (fronts[challenger].rank < fronts[best].rank ||
                    (fronts[challenger].rank == fronts[best].rank &&
                     fronts[challenger].crowding > fronts[best].crowding)) {
                    best = challenger;
                }
            }
            return best;
        };

        std::size_t parent1_idx = nsga2_tournament(idx_dist(rng_));
        std::size_t parent2_idx = select_partner_idx(parent1_idx);

        const auto& eval1 = evaluated[parent1_idx];
        const auto& eval2 = evaluated[parent2_idx];
        const auto* g1 = findGenome(current_genomes, eval1.genome_id);
        const auto* g2 = findGenome(current_genomes, eval2.genome_id);

        if (g1 && g2) {
            const auto* ng1 = dynamic_cast<const NeatGenome*>(g1);
            const auto* ng2 = dynamic_cast<const NeatGenome*>(g2);
            if (ng1 && ng2) {
                GenomeId child_id{++max_id.value};
                auto child = crossover(*ng1, *ng2, eval1, eval2, child_id);
                mutate(child);
                next_gen.push_back(std::make_unique<NeatGenome>(std::move(child)));
            } else {
                auto child = g1->clone();
                if (auto* neat = dynamic_cast<NeatGenome*>(child.get())) {
                    neat->setId(GenomeId{++max_id.value});
                }
                next_gen.push_back(std::move(child));
            }
        }
    }

    return next_gen;
}

// --- Mutation methods (matching NeatGenome API) ---

const NeatGenome* Nsga2Evolution::findGenome(const std::vector<IGenome*>& current_genomes, GenomeId id) const {
    for (const auto* g : current_genomes) {
        if (g != nullptr && g->id() == id) return dynamic_cast<const NeatGenome*>(g);
    }
    return nullptr;
}

NeatGenome Nsga2Evolution::crossover(const NeatGenome& a, const NeatGenome& b,
                                      const EvaluatedIndividual& a_eval,
                                      const EvaluatedIndividual& b_eval,
                                      GenomeId child_id) {
    const NeatGenome* primary = &a;
    const NeatGenome* secondary = &b;
    if (a_eval.fitness < b_eval.fitness) std::swap(primary, secondary);

    // NEAT crossover: match genes by innovation, inherit from primary for matching
    std::bernoulli_distribution pick_half(0.5);
    std::bernoulli_distribution reenable(0.25);

    std::unordered_map<std::uint64_t, ConnectionGene> a_conns;
    for (const auto& gene : a.connections()) a_conns.emplace(gene.innovation_id.value, gene);
    std::unordered_map<std::uint64_t, ConnectionGene> b_conns;
    for (const auto& gene : b.connections()) b_conns.emplace(gene.innovation_id.value, gene);

    std::vector<std::uint64_t> innovations;
    for (const auto& [innov, _] : a_conns) { (void)_; innovations.push_back(innov); }
    for (const auto& [innov, _] : b_conns) { (void)_; if (!a_conns.contains(innov)) innovations.push_back(innov); }

    std::vector<ConnectionGene> child_connections;
    for (auto innov : innovations) {
        bool in_a = a_conns.contains(innov);
        bool in_b = b_conns.contains(innov);
        if (in_a && in_b) {
            const auto& gene = pick_half(rng_) ? a_conns[innov] : b_conns[innov];
            child_connections.push_back(gene);
        } else if (in_a) {
            child_connections.push_back(a_conns[innov]);
        } else {
            // Excess/disjoint from secondary: only if equal fitness
            if (std::abs(a_eval.fitness - b_eval.fitness) < 1e-9) {
                if (pick_half(rng_)) child_connections.push_back(b_conns[innov]);
            }
        }
        // Re-enable disabled genes with some probability
        if (!child_connections.empty() && !child_connections.back().enabled && reenable(rng_)) {
            child_connections.back().enabled = true;
        }
    }

    // Inherit nodes from primary
    std::vector<NodeGene> child_nodes = primary->nodes();

    const float child_interval = pick_half(rng_) ? a.decisionInterval() : b.decisionInterval();
    const float child_immunity = pick_half(rng_) ? a.immunityGainRate() : b.immunityGainRate();
    const float child_habitat = pick_half(rng_) ? a.habitatPreference() : b.habitatPreference();
    const float child_activity = pick_half(rng_) ? a.activityPeriod() : b.activityPeriod();
    const float child_diet = pick_half(rng_) ? a.dietPreference() : b.dietPreference();

    auto child = NeatGenome(child_id, a.inputDim(), a.outputDim(),
                            std::move(child_nodes), std::move(child_connections),
                            child_interval);
    child.setImmunityGainRate(child_immunity);
    child.setHabitatPreference(child_habitat);
    child.setActivityPeriod(child_activity);
    child.setDietPreference(child_diet);
    return child;
}

void Nsga2Evolution::mutate(NeatGenome& genome) {
    mutateWeights(genome);
    mutateNodeParameters(genome);
    mutateDecisionInterval(genome);
    mutateImmunityGainRate(genome);
    mutateBehavioralGenes(genome);
    addConnectionMutation(genome);
    addNodeMutation(genome);
    toggleConnectionMutation(genome);
    addModulatoryNodeMutation(genome);
}

void Nsga2Evolution::mutateWeights(NeatGenome& genome) {
    std::bernoulli_distribution perturb(weight_perturb_rate_);
    std::bernoulli_distribution reset(weight_reset_rate_);
    std::normal_distribution<float> perturb_dist(0.0f, static_cast<float>(weight_perturb_sigma_));
    for (auto& conn : genome.connections()) {
        if (reset(rng_)) {
            std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
            conn.weight = dist(rng_);
        } else if (perturb(rng_)) {
            conn.weight += perturb_dist(rng_);
            conn.weight = std::clamp(conn.weight, -5.0f, 5.0f);
        }
    }
}

void Nsga2Evolution::mutateNodeParameters(NeatGenome& genome) {
    std::bernoulli_distribution perturb(weight_perturb_rate_);
    std::bernoulli_distribution reset(weight_reset_rate_);
    std::normal_distribution<float> perturb_dist(0.0f, static_cast<float>(weight_perturb_sigma_ * 0.25));
    std::uniform_real_distribution<float> reset_bias(-1.0f, 1.0f);
    for (auto& node : genome.nodes()) {
        if (node.type != NodeGeneType::Hidden && node.type != NodeGeneType::Modulatory) continue;
        if (reset(rng_)) {
            node.bias = reset_bias(rng_);
        } else if (perturb(rng_)) {
            node.bias += perturb_dist(rng_);
            node.bias = std::clamp(node.bias, -5.0f, 5.0f);
        }
        if (perturb(rng_)) {
            node.time_constant += perturb_dist(rng_);
            node.time_constant = std::clamp(node.time_constant, tau_min_, tau_max_);
        }
    }
}

void Nsga2Evolution::addConnectionMutation(NeatGenome& genome) {
    if (genome.nodes().size() < 2) return;

    std::uniform_int_distribution<std::size_t> source_dist(0, genome.nodes().size() - 1);
    std::uniform_int_distribution<std::size_t> target_dist(0, genome.nodes().size() - 1);

    for (std::size_t attempt = 0; attempt < 24; ++attempt) {
        const auto& source = genome.nodes()[source_dist(rng_)];
        const auto& target = genome.nodes()[target_dist(rng_)];
        if (target.type == NodeGeneType::Input) continue;
        if (genome.hasConnection(source.id, target.id)) continue;

        std::normal_distribution<float> weight_dist(0.0f, 1.0f);
        genome.connections().push_back(ConnectionGene{
            source.id, target.id, weight_dist(rng_), true,
            innovation_tracker_.nextConnectionInnovation(source.id, target.id),
        });
        return;
    }
}

void Nsga2Evolution::addNodeMutation(NeatGenome& genome) {
    std::vector<std::size_t> enabled_indices;
    for (std::size_t i = 0; i < genome.connections().size(); ++i) {
        if (genome.connections()[i].enabled) enabled_indices.push_back(i);
    }
    if (enabled_indices.empty()) return;

    std::uniform_int_distribution<std::size_t> index_dist(0, enabled_indices.size() - 1);
    const std::size_t split_index = enabled_indices[index_dist(rng_)];
    const ConnectionGene split = genome.connections()[split_index];
    genome.connections()[split_index].enabled = false;

    const NodeId new_node_id = innovation_tracker_.nextNodeId();
    std::normal_distribution<float> bias_dist(0.0f, 1.0f);
    std::uniform_real_distribution<float> tau_dist(tau_min_, tau_max_);
    genome.nodes().push_back(NodeGene{
        new_node_id, NodeGeneType::Hidden, bias_dist(rng_), 1.0f, tau_dist(rng_)
    });

    genome.connections().push_back(ConnectionGene{
        split.from, new_node_id, 1.0f, true,
        innovation_tracker_.nextConnectionInnovation(split.from, new_node_id),
    });
    genome.connections().push_back(ConnectionGene{
        new_node_id, split.to, split.weight, true,
        innovation_tracker_.nextConnectionInnovation(new_node_id, split.to),
    });
}

void Nsga2Evolution::toggleConnectionMutation(NeatGenome& genome) {
    if (genome.connections().empty()) return;
    std::uniform_int_distribution<std::size_t> index_dist(0, genome.connections().size() - 1);
    auto& connection = genome.connections()[index_dist(rng_)];
    connection.enabled = !connection.enabled;
}

void Nsga2Evolution::addModulatoryNodeMutation(NeatGenome& genome) {
    if (genome.nodes().size() < 2) return;

    std::vector<std::size_t> hidden_indices;
    for (std::size_t i = 0; i < genome.nodes().size(); ++i) {
        if (genome.nodes()[i].type == NodeGeneType::Hidden) hidden_indices.push_back(i);
    }
    if (hidden_indices.empty()) return;

    std::uniform_int_distribution<std::size_t> h_dist(0, hidden_indices.size() - 1);
    const std::size_t target_idx = hidden_indices[h_dist(rng_)];
    const NodeId target_id = genome.nodes()[target_idx].id;

    const NodeId mod_node_id = innovation_tracker_.nextNodeId();
    std::normal_distribution<float> bias_dist(0.0f, 1.0f);
    std::uniform_real_distribution<float> tau_dist(tau_min_, tau_max_);
    genome.nodes().push_back(NodeGene{
        mod_node_id, NodeGeneType::Modulatory, bias_dist(rng_), 1.0f, tau_dist(rng_)
    });

    genome.connections().push_back(ConnectionGene{
        mod_node_id, target_id, 0.5f, true,
        innovation_tracker_.nextConnectionInnovation(mod_node_id, target_id),
    });
}

void Nsga2Evolution::mutateDecisionInterval(NeatGenome& genome) {
    std::bernoulli_distribution perturb(weight_perturb_rate_);
    std::bernoulli_distribution do_reset(weight_reset_rate_);
    std::normal_distribution<float> perturb_dist(0.0f, 2.0f);
    std::uniform_real_distribution<float> reset_dist(decision_interval_min_, decision_interval_max_);
    if (do_reset(rng_)) {
        genome.setDecisionInterval(reset_dist(rng_));
    } else if (perturb(rng_)) {
        float val = genome.decisionInterval() + perturb_dist(rng_);
        genome.setDecisionInterval(std::clamp(val, decision_interval_min_, decision_interval_max_));
    }
}

void Nsga2Evolution::mutateImmunityGainRate(NeatGenome& genome) {
    std::bernoulli_distribution perturb(weight_perturb_rate_);
    std::bernoulli_distribution do_reset(weight_reset_rate_);
    std::normal_distribution<float> perturb_dist(0.0f, 0.015f);
    std::uniform_real_distribution<float> reset_dist(0.03f, 0.12f);
    if (do_reset(rng_)) {
        genome.setImmunityGainRate(reset_dist(rng_));
    } else if (perturb(rng_)) {
        float rate = genome.immunityGainRate() + perturb_dist(rng_);
        genome.setImmunityGainRate(std::clamp(rate, 0.01f, 0.5f));
    }
}

void Nsga2Evolution::mutateBehavioralGenes(NeatGenome& genome) {
    std::bernoulli_distribution perturb(weight_perturb_rate_);
    std::normal_distribution<float> perturb_dist(0.0f, 0.05f);
    std::uniform_real_distribution<float> reset_dist(0.0f, 1.0f);

    auto mutate_gene = [&](float current, auto setter) {
        float value = current;
        if (std::bernoulli_distribution(weight_reset_rate_)(rng_)) {
            value = reset_dist(rng_);
        } else if (perturb(rng_)) {
            value = std::clamp(current + perturb_dist(rng_), 0.0f, 1.0f);
        }
        setter(value);
    };

    mutate_gene(genome.habitatPreference(), [&](float value) { genome.setHabitatPreference(value); });
    mutate_gene(genome.activityPeriod(), [&](float value) { genome.setActivityPeriod(value); });
    mutate_gene(genome.dietPreference(), [&](float value) { genome.setDietPreference(value); });
}

void Nsga2Evolution::writeParetoFront(const std::vector<EvaluatedIndividual>& evaluated,
                                       const std::string& output_file) const {
    if (evaluated.empty()) return;

    auto assignments = const_cast<Nsga2Evolution*>(this)->nonDominatedSort(evaluated);

    std::ofstream out(output_file, std::ios::out | std::ios::app);
    if (!out.is_open()) return;

    for (std::size_t i = 0; i < evaluated.size(); ++i) {
        if (assignments[i].rank > 2) continue;  // Only top 3 fronts
        out << assignments[i].rank << ','
            << evaluated[i].fitness;
        for (const auto& obj : evaluated[i].objectives) {
            out << ',' << obj;
        }
        out << ',' << assignments[i].crowding << '\n';
    }
    out.flush();
}

}  // namespace neuro
