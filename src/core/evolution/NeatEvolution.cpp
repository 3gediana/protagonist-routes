#include "core/evolution/NeatEvolution.h"

#include "core/brain/neat/NeatGenome.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace neuro {

namespace {

bool betterIndividual(const EvaluatedIndividual& lhs, const EvaluatedIndividual& rhs) {
    if (lhs.fitness != rhs.fitness) {
        return lhs.fitness > rhs.fitness;
    }
    return lhs.complexity < rhs.complexity;
}

void validateGenomeStructure(const NeatGenome& genome, const char* phase) {
    std::unordered_map<NodeId, bool> node_ids;
    for (const auto& node : genome.nodes()) {
        node_ids.emplace(node.id, true);
    }

    for (const auto& connection : genome.connections()) {
        if (!node_ids.contains(connection.from) || !node_ids.contains(connection.to)) {
            std::ostringstream os;
            os << "NeatEvolution invalid genome during " << phase
               << ": genome=" << genome.id().value
               << " missing node for connection innovation=" << connection.innovation_id.value
               << " from=" << connection.from
               << " to=" << connection.to
               << " node_count=" << genome.nodeCount()
               << " connection_count=" << genome.connectionCount();
            throw std::runtime_error(os.str());
        }
    }
}

}  // namespace

NeatEvolution::NeatEvolution(double elite_ratio,
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
                             float decision_interval_min,
                             float decision_interval_max)
    : elite_ratio_(elite_ratio),
      tournament_size_(tournament_size == 0 ? 1 : tournament_size),
      weight_perturb_rate_(weight_perturb_rate),
      weight_reset_rate_(weight_reset_rate),
      add_node_rate_(add_node_rate),
      add_connection_rate_(add_connection_rate),
      toggle_connection_rate_(toggle_connection_rate),
      add_modulatory_rate_(add_modulatory_rate),
      weight_perturb_sigma_(weight_perturb_sigma),
      tau_min_(tau_min),
      tau_max_(tau_max),
      target_species_min_(3),
      target_species_max_(10),
      compatibility_adjustment_step_(0.25),
      decision_interval_min_(decision_interval_min),
      decision_interval_max_(decision_interval_max),
      rng_(seed == 0 ? std::random_device{}() : seed),
      innovation_tracker_(1, 1),
      speciation_(CompatibilityDistance(c1_excess, c2_disjoint, c3_weight), compatibility_threshold) {}

std::vector<std::unique_ptr<IGenome>> NeatEvolution::reproduce(
    const std::vector<EvaluatedIndividual>& evaluated,
    const std::vector<IGenome*>& current_genomes,
    const std::vector<std::pair<GenomeId, GenomeId>>* mating_pairs) {
    if (evaluated.empty() || current_genomes.empty()) {
        return {};
    }

    innovation_tracker_.beginGeneration();

    auto species_states = buildSpeciesStates(evaluated, current_genomes);
    if (species_states.empty()) {
        return {};
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

    GenomeId max_id = kInvalidGenomeId;
    for (const IGenome* genome : current_genomes) {
        if (genome != nullptr && genome->id().value > max_id.value) {
            max_id = genome->id();
        }
    }

    const auto spawn_counts = allocateSpawnCounts(species_states, evaluated.size());
    for (std::size_t i = 0; i < species_states.size(); ++i) {
        species_states[i].spawn_count = spawn_counts[i];
    }

    std::vector<std::unique_ptr<IGenome>> next_generation;
    next_generation.reserve(evaluated.size());

    auto select_parent = [&](const std::vector<EvaluatedIndividual>& members) -> const EvaluatedIndividual& {
        if (members.empty()) {
            throw std::runtime_error("Cannot select parent from empty species");
        }

        std::uniform_int_distribution<std::size_t> index_dist(0, members.size() - 1);
        const EvaluatedIndividual* best = &members[index_dist(rng_)];
        for (std::size_t i = 1; i < tournament_size_; ++i) {
            const EvaluatedIndividual* candidate = &members[index_dist(rng_)];
            if (betterIndividual(*candidate, *best)) {
                best = candidate;
            }
        }
        return *best;
    };

    auto select_partner = [&](const EvaluatedIndividual& primary,
                              const std::vector<EvaluatedIndividual>& members) -> const EvaluatedIndividual& {
        if (mate_lookup.empty() || !std::bernoulli_distribution(0.75)(rng_)) {
            return select_parent(members);
        }

        const auto range = mate_lookup.equal_range(primary.genome_id);
        if (range.first == range.second) {
            return select_parent(members);
        }

        std::vector<const EvaluatedIndividual*> candidates;
        for (auto it = range.first; it != range.second; ++it) {
            for (const auto& member : members) {
                if (member.genome_id == it->second && member.genome_id != primary.genome_id) {
                    candidates.push_back(&member);
                    break;
                }
            }
        }

        if (candidates.empty()) {
            return select_parent(members);
        }

        std::uniform_int_distribution<std::size_t> index_dist(0, candidates.size() - 1);
        const EvaluatedIndividual* best = candidates[index_dist(rng_)];
        for (std::size_t i = 1; i < tournament_size_; ++i) {
            const EvaluatedIndividual* candidate = candidates[index_dist(rng_)];
            if (betterIndividual(*candidate, *best)) {
                best = candidate;
            }
        }
        return *best;
    };

    for (auto& species_state : species_states) {
        if (species_state.spawn_count == 0 || species_state.members.empty()) {
            continue;
        }

        const std::size_t elite_count = std::min(
            species_state.spawn_count,
            std::max<std::size_t>(1, static_cast<std::size_t>(std::floor(species_state.members.size() * elite_ratio_))));

        for (std::size_t i = 0; i < elite_count; ++i) {
            const auto* elite = findGenome(current_genomes, species_state.members[i].genome_id);
            if (elite == nullptr) {
                throw std::runtime_error("Neat elite genome not found");
            }

            auto child = elite->clone();
            auto* child_neat = dynamic_cast<NeatGenome*>(child.get());
            if (child_neat == nullptr) {
                throw std::runtime_error("Cloned elite is not a NeatGenome");
            }
            child_neat->setId(GenomeId{++max_id.value});
            validateGenomeStructure(*child_neat, "elite_clone");
            next_generation.push_back(std::move(child));
        }

        for (std::size_t produced = elite_count; produced < species_state.spawn_count; ++produced) {
            const auto& parent_a_eval = select_parent(species_state.members);
            const auto& parent_b_eval = select_partner(parent_a_eval, species_state.members);
            const auto* parent_a = findGenome(current_genomes, parent_a_eval.genome_id);
            const auto* parent_b = findGenome(current_genomes, parent_b_eval.genome_id);
            if (parent_a == nullptr || parent_b == nullptr) {
                throw std::runtime_error("Neat tournament parent genome not found");
            }

            NeatGenome child = crossover(*parent_a, *parent_b, parent_a_eval, parent_b_eval, GenomeId{++max_id.value});
            validateGenomeStructure(child, "post_crossover");
            mutate(child);
            validateGenomeStructure(child, "post_mutation");
            next_generation.push_back(std::make_unique<NeatGenome>(std::move(child)));
        }
    }

    if (next_generation.size() > evaluated.size()) {
        next_generation.resize(evaluated.size());
    }

    while (next_generation.size() < evaluated.size()) {
        const auto best_it = std::max_element(evaluated.begin(), evaluated.end(), [](const auto& lhs, const auto& rhs) {
            return betterIndividual(rhs, lhs);
        });
        const auto* genome = findGenome(current_genomes, best_it->genome_id);
        if (genome == nullptr) {
            throw std::runtime_error("Best genome not found while backfilling population");
        }
        auto child = genome->clone();
        auto* child_neat = dynamic_cast<NeatGenome*>(child.get());
        child_neat->setId(GenomeId{++max_id.value});
        validateGenomeStructure(*child_neat, "backfill_clone");
        next_generation.push_back(std::move(child));
    }

    return next_generation;
}

const char* NeatEvolution::name() const {
    return "neat";
}

InnovationTracker& NeatEvolution::innovationTracker() {
    return innovation_tracker_;
}

std::vector<SpeciesGroup> NeatEvolution::assignSpecies(const std::vector<IGenome*>& current_genomes) {
    std::vector<const NeatGenome*> neat_genomes;
    neat_genomes.reserve(current_genomes.size());
    for (const IGenome* genome : current_genomes) {
        const auto* neat_genome = dynamic_cast<const NeatGenome*>(genome);
        if (neat_genome == nullptr) {
            throw std::runtime_error("NeatEvolution received non-NEAT genome");
        }
        neat_genomes.push_back(neat_genome);
    }

    last_species_groups_ = speciation_.assign(neat_genomes);
    speciation_.adjustCompatibilityThreshold(
        target_species_min_,
        target_species_max_,
        last_species_groups_.size(),
        compatibility_adjustment_step_);
    return last_species_groups_;
}

const std::vector<SpeciesGroup>& NeatEvolution::lastSpeciesGroups() const {
    return last_species_groups_;
}

const NeatGenome* NeatEvolution::findGenome(const std::vector<IGenome*>& current_genomes, GenomeId id) const {
    for (const IGenome* genome : current_genomes) {
        if (genome != nullptr && genome->id() == id) {
            return dynamic_cast<const NeatGenome*>(genome);
        }
    }
    return nullptr;
}

std::vector<NeatEvolution::SpeciesState> NeatEvolution::buildSpeciesStates(
    const std::vector<EvaluatedIndividual>& evaluated,
    const std::vector<IGenome*>& current_genomes) {
    const auto groups = assignSpecies(current_genomes);
    std::vector<SpeciesState> states;
    states.reserve(groups.size());
    std::unordered_map<GenomeId, std::size_t> genome_to_state_index;

    for (std::size_t i = 0; i < groups.size(); ++i) {
        states.push_back(SpeciesState{groups[i], {}, 0.0, 0});
        for (const GenomeId genome_id : groups[i].members) {
            genome_to_state_index.emplace(genome_id, i);
        }
    }

    for (const auto& item : evaluated) {
        const auto state_it = genome_to_state_index.find(item.genome_id);
        if (state_it == genome_to_state_index.end()) {
            throw std::runtime_error("Evaluated genome missing from speciation output");
        }
        states[state_it->second].members.push_back(item);
    }

    for (auto& state : states) {
        std::sort(state.members.begin(), state.members.end(), betterIndividual);
        const double divisor = state.members.size() >= 2 ? static_cast<double>(state.members.size()) : 1.0;
        double adjusted_sum = 0.0;
        for (const auto& member : state.members) {
            adjusted_sum += std::max(0.0, member.fitness / divisor);
        }
        state.adjusted_total_fitness = adjusted_sum;
    }

    return states;
}

std::vector<std::size_t> NeatEvolution::allocateSpawnCounts(const std::vector<SpeciesState>& species,
                                                            std::size_t population_size) const {
    std::vector<std::size_t> counts(species.size(), 0);
    if (species.empty() || population_size == 0) {
        return counts;
    }

    const double total_adjusted = std::accumulate(
        species.begin(), species.end(), 0.0, [](double sum, const SpeciesState& state) {
            return sum + state.adjusted_total_fitness;
        });

    if (total_adjusted <= 0.0) {
        for (std::size_t i = 0; i < population_size; ++i) {
            ++counts[i % counts.size()];
        }
        return counts;
    }

    struct FractionalSpawn {
        std::size_t index;
        double remainder;
    };

    std::vector<FractionalSpawn> fractions;
    fractions.reserve(species.size());
    std::size_t allocated = 0;
    if (species.size() <= population_size) {
        for (std::size_t i = 0; i < species.size(); ++i) {
            counts[i] = 1;
            ++allocated;
        }
    }

    for (std::size_t i = 0; i < species.size(); ++i) {
        const double exact = species[i].adjusted_total_fitness / total_adjusted * static_cast<double>(population_size);
        const std::size_t floor_exact = static_cast<std::size_t>(std::floor(exact));
        if (floor_exact > counts[i]) {
            allocated += floor_exact - counts[i];
            counts[i] = floor_exact;
        }
        fractions.push_back(FractionalSpawn{i, exact - static_cast<double>(floor_exact)});
    }

    std::sort(fractions.begin(), fractions.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.remainder > rhs.remainder;
    });

    std::size_t fraction_index = 0;
    while (allocated < population_size && !fractions.empty()) {
        ++counts[fractions[fraction_index].index];
        ++allocated;
        fraction_index = (fraction_index + 1) % fractions.size();
    }

    fraction_index = fractions.size();
    while (allocated > population_size && !fractions.empty()) {
        if (fraction_index == 0) {
            fraction_index = fractions.size();
        }
        --fraction_index;
        auto& count = counts[fractions[fraction_index].index];
        if (count == 0) {
            continue;
        }
        if (species.size() <= population_size && count == 1) {
            continue;
        }
        --count;
        --allocated;
    }

    return counts;
}

NeatGenome NeatEvolution::crossover(const NeatGenome& a,
                                    const NeatGenome& b,
                                    const EvaluatedIndividual& a_eval,
                                    const EvaluatedIndividual& b_eval,
                                    GenomeId child_id) {
    const EvaluatedIndividual* primary_eval = &a_eval;
    const EvaluatedIndividual* secondary_eval = &b_eval;
    const NeatGenome* primary = &a;
    const NeatGenome* secondary = &b;
    if (betterIndividual(b_eval, a_eval)) {
        primary_eval = &b_eval;
        secondary_eval = &a_eval;
        primary = &b;
        secondary = &a;
    }

    const bool equal_fitness = std::abs(primary_eval->fitness - secondary_eval->fitness) < 1.0e-9
        && primary_eval->complexity == secondary_eval->complexity;
    std::bernoulli_distribution pick_half(0.5);
    std::bernoulli_distribution reenable(0.25);

    std::unordered_map<std::uint64_t, ConnectionGene> a_connections;
    for (const auto& gene : a.connections()) {
        a_connections.emplace(gene.innovation_id.value, gene);
    }
    std::unordered_map<std::uint64_t, ConnectionGene> b_connections;
    for (const auto& gene : b.connections()) {
        b_connections.emplace(gene.innovation_id.value, gene);
    }

    std::vector<std::uint64_t> innovations;
    innovations.reserve(a_connections.size() + b_connections.size());
    for (const auto& [innovation, gene] : a_connections) {
        (void)gene;
        innovations.push_back(innovation);
    }
    for (const auto& [innovation, gene] : b_connections) {
        (void)gene;
        if (!a_connections.contains(innovation)) {
            innovations.push_back(innovation);
        }
    }
    std::sort(innovations.begin(), innovations.end());

    std::vector<ConnectionGene> child_connections;
    child_connections.reserve(innovations.size());
    for (const auto innovation : innovations) {
        const auto a_it = a_connections.find(innovation);
        const auto b_it = b_connections.find(innovation);
        const bool has_a = a_it != a_connections.end();
        const bool has_b = b_it != b_connections.end();

        if (has_a && has_b) {
            ConnectionGene chosen = pick_half(rng_) ? a_it->second : b_it->second;
            if (!a_it->second.enabled || !b_it->second.enabled) {
                chosen.enabled = reenable(rng_);
            }
            child_connections.push_back(chosen);
            continue;
        }

        if (!equal_fitness) {
            if (has_a && primary == &a) {
                child_connections.push_back(a_it->second);
            } else if (has_b && primary == &b) {
                child_connections.push_back(b_it->second);
            }
            continue;
        }

        if (pick_half(rng_)) {
            if (has_a) {
                child_connections.push_back(a_it->second);
            } else if (has_b) {
                child_connections.push_back(b_it->second);
            }
        }
    }

    if (child_connections.empty() && !primary->connections().empty()) {
        child_connections = primary->connections();
    }

    std::unordered_map<NodeId, NodeGene> node_by_id;
    auto remember_node = [&](const NeatGenome& genome, NodeId node_id) {
        if (node_by_id.contains(node_id)) {
            return;
        }
        if (const auto* node = genome.findNode(node_id); node != nullptr) {
            node_by_id.emplace(node_id, *node);
        }
    };

    for (const auto& node : primary->nodes()) {
        if (node.type == NodeGeneType::Input || node.type == NodeGeneType::Output) {
            node_by_id.emplace(node.id, node);
        }
    }
    for (const auto& node : secondary->nodes()) {
        if ((node.type == NodeGeneType::Input || node.type == NodeGeneType::Output) && !node_by_id.contains(node.id)) {
            node_by_id.emplace(node.id, node);
        }
    }
    for (const auto& gene : child_connections) {
        remember_node(*primary, gene.from);
        remember_node(*primary, gene.to);
        remember_node(*secondary, gene.from);
        remember_node(*secondary, gene.to);
    }

    std::vector<NodeGene> child_nodes;
    child_nodes.reserve(node_by_id.size());
    for (const auto& [node_id, node] : node_by_id) {
        (void)node_id;
        child_nodes.push_back(node);
    }
    std::sort(child_nodes.begin(), child_nodes.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.id < rhs.id;
    });

    const float child_interval = pick_half(rng_) ? primary->decisionInterval() : secondary->decisionInterval();
    const float child_immunity = pick_half(rng_) ? primary->immunityGainRate() : secondary->immunityGainRate();
    const float child_habitat = pick_half(rng_) ? primary->habitatPreference() : secondary->habitatPreference();
    const float child_activity = pick_half(rng_) ? primary->activityPeriod() : secondary->activityPeriod();
    const float child_diet = pick_half(rng_) ? primary->dietPreference() : secondary->dietPreference();

    auto child = NeatGenome(child_id, primary->inputDim(), primary->outputDim(), std::move(child_nodes), std::move(child_connections), child_interval);
    child.setImmunityGainRate(child_immunity);
    child.setHabitatPreference(child_habitat);
    child.setActivityPeriod(child_activity);
    child.setDietPreference(child_diet);
    return child;
}

void NeatEvolution::mutate(NeatGenome& genome) {
    mutateWeights(genome);
    mutateNodeParameters(genome);
    mutateDecisionInterval(genome);
    mutateImmunityGainRate(genome);
    mutateBehavioralGenes(genome);

    std::bernoulli_distribution add_connection(add_connection_rate_);
    std::bernoulli_distribution add_node(add_node_rate_);
    std::bernoulli_distribution toggle(toggle_connection_rate_);
    std::bernoulli_distribution add_modulatory(add_modulatory_rate_);

    if (add_connection(rng_)) {
        addConnectionMutation(genome);
    }
    if (add_node(rng_)) {
        addNodeMutation(genome);
    }
    if (add_modulatory(rng_)) {
        addModulatoryNodeMutation(genome);
    }
    if (toggle(rng_)) {
        toggleConnectionMutation(genome);
    }
}

void NeatEvolution::mutateWeights(NeatGenome& genome) {
    std::bernoulli_distribution perturb(weight_perturb_rate_);
    std::bernoulli_distribution reset(weight_reset_rate_);
    std::normal_distribution<float> perturb_dist(0.0f, static_cast<float>(weight_perturb_sigma_));
    std::normal_distribution<float> reset_dist(0.0f, 1.0f);

    for (auto& connection : genome.connections()) {
        if (reset(rng_)) {
            connection.weight = reset_dist(rng_);
        } else if (perturb(rng_)) {
            connection.weight += perturb_dist(rng_);
        }
    }
}

void NeatEvolution::mutateNodeParameters(NeatGenome& genome) {
    std::bernoulli_distribution perturb(weight_perturb_rate_);
    std::bernoulli_distribution reset(weight_reset_rate_);
    std::normal_distribution<float> perturb_dist(0.0f, static_cast<float>(weight_perturb_sigma_ * 0.25));
    std::normal_distribution<float> bias_reset_dist(0.0f, 1.0f);
    std::uniform_real_distribution<float> tau_reset_dist(tau_min_, tau_max_);

    for (auto& node : genome.nodes()) {
        if (node.type == NodeGeneType::Input) {
            continue;
        }

        if (reset(rng_)) {
            node.bias = bias_reset_dist(rng_);
        } else if (perturb(rng_)) {
            node.bias += perturb_dist(rng_);
        }

        if (reset(rng_)) {
            node.time_constant = tau_reset_dist(rng_);
        } else if (perturb(rng_)) {
            node.time_constant = std::clamp(node.time_constant + perturb_dist(rng_), tau_min_, tau_max_);
        }
    }
}

void NeatEvolution::addConnectionMutation(NeatGenome& genome) {
    if (genome.nodes().size() < 2) {
        return;
    }

    std::uniform_int_distribution<std::size_t> source_dist(0, genome.nodes().size() - 1);
    std::uniform_int_distribution<std::size_t> target_dist(0, genome.nodes().size() - 1);

    for (std::size_t attempt = 0; attempt < 24; ++attempt) {
        const auto& source = genome.nodes()[source_dist(rng_)];
        const auto& target = genome.nodes()[target_dist(rng_)];
        if (target.type == NodeGeneType::Input) {
            continue;
        }
        if (genome.hasConnection(source.id, target.id)) {
            continue;
        }

        std::normal_distribution<float> weight_dist(0.0f, 1.0f);
        genome.connections().push_back(ConnectionGene{
            source.id,
            target.id,
            weight_dist(rng_),
            true,
            innovation_tracker_.nextConnectionInnovation(source.id, target.id),
        });
        return;
    }
}

void NeatEvolution::addNodeMutation(NeatGenome& genome) {
    std::vector<std::size_t> enabled_indices;
    for (std::size_t i = 0; i < genome.connections().size(); ++i) {
        if (genome.connections()[i].enabled) {
            enabled_indices.push_back(i);
        }
    }
    if (enabled_indices.empty()) {
        return;
    }

    std::uniform_int_distribution<std::size_t> index_dist(0, enabled_indices.size() - 1);
    const std::size_t split_index = enabled_indices[index_dist(rng_)];
    const ConnectionGene split = genome.connections()[split_index];
    genome.connections()[split_index].enabled = false;

    const NodeId new_node_id = innovation_tracker_.nextNodeId();
    std::normal_distribution<float> bias_dist(0.0f, 1.0f);
    std::uniform_real_distribution<float> tau_dist(tau_min_, tau_max_);
    genome.nodes().push_back(NodeGene{
        new_node_id,
        NodeGeneType::Hidden,
        bias_dist(rng_),
        1.0f,
        tau_dist(rng_),
    });

    genome.connections().push_back(ConnectionGene{
        split.from,
        new_node_id,
        1.0f,
        true,
        innovation_tracker_.nextConnectionInnovation(split.from, new_node_id),
    });
    genome.connections().push_back(ConnectionGene{
        new_node_id,
        split.to,
        split.weight,
        true,
        innovation_tracker_.nextConnectionInnovation(new_node_id, split.to),
    });
}

void NeatEvolution::toggleConnectionMutation(NeatGenome& genome) {
    if (genome.connections().empty()) {
        return;
    }

    std::uniform_int_distribution<std::size_t> index_dist(0, genome.connections().size() - 1);
    auto& connection = genome.connections()[index_dist(rng_)];
    connection.enabled = !connection.enabled;
}

void NeatEvolution::addModulatoryNodeMutation(NeatGenome& genome) {
    if (genome.nodes().size() < 2) {
        return;
    }

    const NodeId new_node_id = innovation_tracker_.nextNodeId();
    std::normal_distribution<float> bias_dist(0.0f, 1.0f);
    std::uniform_real_distribution<float> tau_dist(tau_min_, tau_max_);

    genome.nodes().push_back(NodeGene{
        new_node_id,
        NodeGeneType::Modulatory,
        bias_dist(rng_),
        1.0f,
        tau_dist(rng_),
    });

    std::uniform_int_distribution<std::size_t> source_dist(0, genome.nodes().size() - 2);
    const auto& source = genome.nodes()[source_dist(rng_)];
    if (source.type != NodeGeneType::Output && source.type != NodeGeneType::Modulatory) {
        genome.connections().push_back(ConnectionGene{
            source.id,
            new_node_id,
            1.0f,
            true,
            innovation_tracker_.nextConnectionInnovation(source.id, new_node_id),
        });
    }

    std::vector<std::size_t> target_candidates;
    for (std::size_t i = 0; i < genome.nodes().size() - 1; ++i) {
        const auto& n = genome.nodes()[i];
        if (n.type == NodeGeneType::Hidden || n.type == NodeGeneType::Output) {
            target_candidates.push_back(i);
        }
    }
    if (!target_candidates.empty()) {
        std::uniform_int_distribution<std::size_t> target_dist(0, target_candidates.size() - 1);
        const auto& target = genome.nodes()[target_candidates[target_dist(rng_)]];
        std::normal_distribution<float> weight_dist(0.0f, 1.0f);
        genome.connections().push_back(ConnectionGene{
            new_node_id,
            target.id,
            weight_dist(rng_),
            true,
            innovation_tracker_.nextConnectionInnovation(new_node_id, target.id),
        });
    }
}

void NeatEvolution::mutateDecisionInterval(NeatGenome& genome) {
    std::bernoulli_distribution perturb(weight_perturb_rate_);
    std::normal_distribution<float> perturb_dist(0.0f, 2.0f);
    std::uniform_real_distribution<float> reset_dist(decision_interval_min_, decision_interval_max_);

    if (reset_dist(rng_) < weight_reset_rate_) {
        genome.setDecisionInterval(reset_dist(rng_));
    } else if (perturb(rng_)) {
        const float new_interval = genome.decisionInterval() + perturb_dist(rng_);
        genome.setDecisionInterval(std::clamp(new_interval, decision_interval_min_, decision_interval_max_));
    }
}

void NeatEvolution::mutateImmunityGainRate(NeatGenome& genome) {
    std::bernoulli_distribution perturb(weight_perturb_rate_);
    std::normal_distribution<float> perturb_dist(0.0f, 0.015f);
    std::uniform_real_distribution<float> reset_dist(0.03f, 0.12f);

    if (std::bernoulli_distribution(weight_reset_rate_)(rng_)) {
        genome.setImmunityGainRate(reset_dist(rng_));
    } else if (perturb(rng_)) {
        const float new_rate = genome.immunityGainRate() + perturb_dist(rng_);
        genome.setImmunityGainRate(new_rate);
    }
}

void NeatEvolution::mutateBehavioralGenes(NeatGenome& genome) {
    std::bernoulli_distribution perturb(weight_perturb_rate_);
    std::normal_distribution<float> perturb_dist(0.0f, 0.05f);
    std::uniform_real_distribution<float> reset_dist(0.0f, 1.0f);

    auto mutate_gene = [&](float current, auto setter, float lo, float hi) {
        float val = current;
        if (std::bernoulli_distribution(weight_reset_rate_)(rng_)) {
            val = reset_dist(rng_);
        } else if (perturb(rng_)) {
            val = std::clamp(current + perturb_dist(rng_), lo, hi);
        }
        setter(val);
    };

    mutate_gene(genome.habitatPreference(), [&](float v) { genome.setHabitatPreference(v); }, 0.0f, 1.0f);
    mutate_gene(genome.activityPeriod(), [&](float v) { genome.setActivityPeriod(v); }, 0.0f, 1.0f);
    mutate_gene(genome.dietPreference(), [&](float v) { genome.setDietPreference(v); }, 0.0f, 1.0f);
}

}  // namespace neuro
