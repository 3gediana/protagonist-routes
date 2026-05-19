#include "runtime/headless/HeadlessRuntime.h"

#include "core/agent/Agent.h"
#include "core/brain/fixed/FixedTopologyBrain.h"
#include "core/brain/fixed/FixedTopologyGenome.h"
#include "core/brain/neat/NeatBrain.h"
#include "core/brain/neat/NeatGenome.h"
#include "core/capability/EatCapability.h"
#include "core/capability/MoveCapability.h"
#include "core/capability/TerrainModifier.h"
#include "core/evolution/ClassicGA.h"
#include "core/evolution/NeatEvolution.h"
#include "core/evolution/Nsga2Evolution.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IBrain.h"
#include "core/interfaces/IEvolutionStrategy.h"
#include "core/interfaces/IGenome.h"
#include "core/logging/Logger.h"
#include "core/observer/FitnessRecorder.h"
#include "core/observer/SpeciesObserver.h"
#include "core/observer/PerformanceObserver.h"
#include "core/observer/ModuleMetricsObserver.h"
#include "core/observer/HeatmapObserver.h"
#include "core/observer/AdvancedStatsObserver.h"
#include "core/snapshot/SnapshotIO.h"
#include "runtime/headless/EvaluationPool.h"
#include "core/perception/NearestFoodPerception.h"
#include "core/perception/SelfPerception.h"
#include "core/perception/TerrainPerception.h"
#include "core/perception/TimeOfDayPerception.h"
#include "core/perception/SeasonPerception.h"
#include "core/perception/InfectionPerception.h"
#include "core/perception/SignalPerception.h"
#include "core/perception/FoodScentPerception.h"
#include "core/reward/FoodReward.h"
#include "core/reward/IntrinsicReward.h"
#include "core/reward/SurvivalReward.h"
#include "core/reward/CombatReward.h"
#include "core/reward/CoopReward.h"
#include "core/reward/NoveltyReward.h"
#include "core/capability/AttackCapability.h"
#include "core/capability/HebbianLearning.h"
#include "core/capability/EmitSignalCapability.h"
#include "core/capability/PredatorHuntCapability.h"
#include "core/capability/FleeCapability.h"
#include "core/world/World2D.h"
#include "core/world/layers/AgentLayer.h"
#include "core/world/layers/ResourceLayer.h"
#include "core/world/layers/terrain/TerrainLayer.h"
#include "core/world/layers/TimeOfDayLayer.h"
#include "core/world/layers/SeasonLayer.h"
#include "core/world/layers/DiseaseLayer.h"
#include "core/world/layers/signal/SignalLayer.h"
#include "core/world/layers/HeavyResourceLayer.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace neuro::runtime {

namespace {

struct RuntimeParams {
    std::uint32_t random_seed;
    std::size_t population_size;
    std::size_t input_dim;
    std::size_t hidden_dim;
    std::size_t output_dim;
    std::string brain_type;
    std::size_t generations;
    std::size_t episode_ticks;
    float tick_dt;
    Vec2 world_size;
    std::size_t resource_count;
    float resource_amount;
    std::size_t resource_respawn_ticks;
    float initial_energy;
    float base_metabolism;
    float decision_cost;
    float movement_cost_factor;
    float death_threshold;
    float max_speed;
    float eat_radius;
    std::size_t exploration_grid_size;
    double survival_weight;
    double food_weight;
    double intrinsic_weight;
    double elite_ratio;
    std::size_t tournament_size;
    double weight_perturb_rate;
    double weight_reset_rate;
    double add_node_rate;
    double add_connection_rate;
    double toggle_connection_rate;
    double add_modulatory_rate;
    double weight_perturb_sigma;
    double compatibility_threshold;
    double c1_excess;
    double c2_disjoint;
    double c3_weight;
    float tau_min;
    float tau_max;
    float decision_interval_min;
    float decision_interval_max;
    std::size_t eval_threads;
    bool terrain_enabled;
    TerrainParams terrain_params;
    bool combat_enabled;
    float attack_range;
    float damage_factor;
    float energy_transfer_rate;
    float attack_cost;
    float attack_threshold;
    float combat_attack_reward;
    float combat_kill_bonus;
    float combat_death_penalty;
    bool lifetime_learning_enabled;
    float hebbian_learning_rate;
    bool lamarck_mode;
    bool day_night_enabled;
    float day_length_ticks;
    bool season_enabled;
    std::size_t ticks_per_season;
    bool disease_enabled;
    float infection_radius;
    float infection_prob;
    float infection_duration;
    float death_prob;
    float initial_infection_rate;
    float infection_perception_radius;
    bool dynamic_resources;
    float resource_regen_rate;
    bool signal_enabled;
    std::size_t signal_channels;
    float signal_decay_rate;
    float signal_diffusion_rate;
    std::size_t signal_grid_resolution;
    float signal_perception_radius;
    float signal_emit_cost;
    bool coop_enabled;
    std::size_t heavy_resource_count;
    float heavy_resource_amount;
    std::size_t heavy_required_workers;
    std::size_t heavy_required_ticks;
    float heavy_collect_radius;
    double coop_weight;
    double coop_proximity_bonus;
    bool novelty_enabled;
    double novelty_weight;
    std::size_t novelty_k_neighbors;
    float novelty_threshold;
    bool pred_enabled;
    float pred_ratio;
    float pred_hunt_radius;
    float pred_damage;
    float pred_energy_gain;
    float pred_hunt_cost;
    float pred_flee_radius;
    float pred_flee_boost;
    float pred_flee_cost;
    bool curriculum_enabled;
    std::size_t curriculum_stage1_gen;
    std::size_t curriculum_stage2_gen;
    std::size_t curriculum_stage3_gen;
    bool nsga2_enabled;
    std::size_t snapshot_interval;  // auto-snapshot every N generations (0 = disabled)
    std::string snapshot_label;
    std::string resume_from;       // path to snapshot dir to resume from
    bool terrain_mod_enabled;
    float terrain_mod_dig_rate;
    float terrain_mod_build_rate;
    float terrain_mod_energy_cost;
};

struct GenerationStats {
    double min_fitness;
    double avg_fitness;
    double best_fitness;
    std::size_t species_count;
    double avg_complexity;
    double avg_modulatory_nodes;
    double avg_immunity_gain_rate;
};

std::string makeTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm{};
#ifdef _WIN32
    localtime_s(&local_tm, &now_time);
#else
    localtime_r(&now_time, &local_tm);
#endif
    std::ostringstream os;
    os << std::put_time(&local_tm, "%Y%m%d-%H%M%S");
    return os.str();
}

RuntimeParams loadParams(const config::Config& config) {
    const auto world_size = config.getArray<double>("world.size");
    if (world_size.size() != 2) {
        throw std::runtime_error("world.size must have exactly 2 elements");
    }

    RuntimeParams params{};
    params.random_seed = static_cast<std::uint32_t>(config.getOr<int>("runtime.random_seed", 42));
    params.population_size = config.getOr<std::size_t>("population.size", 100);
    params.input_dim = config.getOr<std::size_t>("population.input_dim", 5);
    params.hidden_dim = config.getOr<std::size_t>("brain.hidden_layer_size", 10);
    params.output_dim = config.getOr<std::size_t>("population.output_dim", 2);
    params.brain_type = config.getOr<std::string>("brain.type", "fixed");
    params.generations = config.getOr<std::size_t>("evolution.generations", 100);
    params.episode_ticks = config.getOr<std::size_t>("evolution.episode_ticks", 3600);
    params.tick_dt = 1.0f;
    params.world_size = Vec2{static_cast<float>(world_size[0]), static_cast<float>(world_size[1])};
    params.resource_count = config.getOr<std::size_t>("world.resources.count", 40);
    params.resource_amount = config.getOr<float>("world.resources.amount", 20.0f);
    params.resource_respawn_ticks = config.getOr<std::size_t>("world.resources.respawn_ticks", 300);
    params.initial_energy = config.getOr<float>("energy.initial", 100.0f);
    params.base_metabolism = config.getOr<float>("energy.base_metabolism", 0.001f);
    params.decision_cost = config.getOr<float>("energy.decision_cost", 0.005f);
    params.movement_cost_factor = config.getOr<float>("energy.movement_cost_factor", 0.001f);
    params.death_threshold = config.getOr<float>("energy.death_threshold", 0.0f);
    params.max_speed = config.getOr<float>("agent.max_speed", 2.0f);
    params.eat_radius = config.getOr<float>("agent.eat_radius", 4.0f);
    params.exploration_grid_size = config.getOr<std::size_t>("agent.exploration_grid_size", 20);
    params.survival_weight = config.getOr<double>("reward.survival_weight", 1.0);
    params.food_weight = config.getOr<double>("reward.food_weight", 10.0);
    params.intrinsic_weight = config.getOr<double>("reward.intrinsic_weight", 5.0);
    params.elite_ratio = config.getOr<double>("evolution.selection.elite_ratio", 0.10);
    params.tournament_size = config.getOr<std::size_t>("evolution.selection.tournament_size", 5);
    params.weight_perturb_rate = config.getOr<double>("evolution.mutation.weight_perturb_rate", 0.80);
    params.weight_reset_rate = config.getOr<double>("evolution.mutation.weight_reset_rate", 0.05);
    params.add_node_rate = config.getOr<double>("evolution.mutation.add_node_rate", 0.03);
    params.add_connection_rate = config.getOr<double>("evolution.mutation.add_connection_rate", 0.05);
    params.toggle_connection_rate = config.getOr<double>("evolution.mutation.toggle_connection_rate", 0.01);
    params.add_modulatory_rate = config.getOr<double>("evolution.mutation.add_modulatory_rate", 0.01);
    params.weight_perturb_sigma = config.getOr<double>("evolution.mutation.weight_perturb_sigma", 0.5);
    params.compatibility_threshold = config.getOr<double>("evolution.speciation.compatibility_threshold", 3.0);
    params.c1_excess = config.getOr<double>("evolution.speciation.c1_excess", 1.0);
    params.c2_disjoint = config.getOr<double>("evolution.speciation.c2_disjoint", 1.0);
    params.c3_weight = config.getOr<double>("evolution.speciation.c3_weight", 0.4);
    params.tau_min = config.getOr<float>("brain.ctrnn.tau_min", 0.1f);
    params.tau_max = config.getOr<float>("brain.ctrnn.tau_max", 5.0f);
    params.decision_interval_min = config.getOr<float>("brain.decision_interval_min", 1.0f);
    params.decision_interval_max = config.getOr<float>("brain.decision_interval_max", 30.0f);
    params.eval_threads = config.getOr<std::size_t>("runtime.eval_threads", 0);
    params.terrain_enabled = config.getOr<bool>("world.terrain.enabled", false);
    params.terrain_params.height_scale = config.getOr<float>("world.terrain.height_scale", 30.0f);
    params.terrain_params.slope_resistance = config.getOr<float>("world.terrain.slope_resistance", 0.6f);
    params.terrain_params.cave_density = config.getOr<float>("world.terrain.cave_density", 0.05f);
    params.terrain_params.cave_shelter_factor = config.getOr<float>("world.terrain.cave_shelter_factor", 0.5f);
    params.terrain_params.seed = static_cast<std::uint32_t>(config.getOr<int>("world.terrain.seed", 42));
    params.terrain_params.grid_resolution = config.getOr<std::size_t>("world.terrain.grid_resolution", 64);
    params.terrain_params.perlin_frequency = config.getOr<float>("world.terrain.perlin_frequency", 4.0f);
    params.terrain_params.perlin_persistence = config.getOr<float>("world.terrain.perlin_persistence", 0.5f);
    params.terrain_params.perlin_octaves = config.getOr<int>("world.terrain.perlin_octaves", 4);
    if (params.terrain_enabled) {
        params.input_dim += TerrainPerception::kDim;
    }
    params.combat_enabled = config.getOr<bool>("combat.enabled", false);
    params.attack_range = config.getOr<float>("combat.attack_range", 5.0f);
    params.damage_factor = config.getOr<float>("combat.damage_factor", 2.0f);
    params.energy_transfer_rate = config.getOr<float>("combat.energy_transfer_rate", 0.5f);
    params.attack_cost = config.getOr<float>("combat.attack_cost", 0.05f);
    params.attack_threshold = config.getOr<float>("combat.attack_threshold", 0.5f);
    params.combat_attack_reward = config.getOr<float>("combat.attack_reward", 2.0f);
    params.combat_kill_bonus = config.getOr<float>("combat.kill_bonus", 10.0f);
    params.combat_death_penalty = config.getOr<float>("combat.death_penalty", 5.0f);
    if (params.combat_enabled) {
        params.output_dim += 3;
    }
    params.lifetime_learning_enabled = config.getOr<bool>("learning.enabled", false);
    params.hebbian_learning_rate = config.getOr<float>("learning.hebbian_rate", 0.002f);
    params.lamarck_mode = config.getOr<bool>("learning.lamarck_mode", false);
    params.day_night_enabled = config.getOr<bool>("world.day_night.enabled", false);
    params.day_length_ticks = config.getOr<float>("world.day_night.day_length_ticks", 600.0f);
    if (params.day_night_enabled) {
        params.input_dim += TimeOfDayPerception::kDim;
    }
    params.season_enabled = config.getOr<bool>("world.season.enabled", false);
    params.ticks_per_season = config.getOr<std::size_t>("world.season.ticks_per_season", 150);
    if (params.season_enabled) {
        params.input_dim += SeasonPerception::kDim;
    }
    params.disease_enabled = config.getOr<bool>("world.disease.enabled", false);
    params.infection_radius = config.getOr<float>("world.disease.infection_radius", 5.0f);
    params.infection_prob = config.getOr<float>("world.disease.infection_prob", 0.1f);
    params.infection_duration = config.getOr<float>("world.disease.infection_duration", 30.0f);
    params.death_prob = config.getOr<float>("world.disease.death_prob", 0.3f);
    params.initial_infection_rate = config.getOr<float>("world.disease.initial_infection_rate", 0.1f);
    params.infection_perception_radius = config.getOr<float>("world.disease.perception_radius", 10.0f);
    if (params.disease_enabled) {
        params.input_dim += InfectionPerception::kDim;
    }
    params.dynamic_resources = config.getOr<bool>("world.resources.dynamic", false);
    params.resource_regen_rate = config.getOr<float>("world.resources.regen_rate", 0.1f);
    params.signal_enabled = config.getOr<bool>("world.signal.enabled", false);
    params.signal_channels = config.getOr<std::size_t>("world.signal.channels", 4);
    params.signal_decay_rate = config.getOr<float>("world.signal.decay_rate", 0.1f);
    params.signal_diffusion_rate = config.getOr<float>("world.signal.diffusion_rate", 0.05f);
    params.signal_grid_resolution = config.getOr<std::size_t>("world.signal.grid_resolution", 32);
    params.signal_perception_radius = config.getOr<float>("world.signal.perception_radius", 15.0f);
    params.signal_emit_cost = config.getOr<float>("world.signal.emit_cost", 0.05f);
    if (params.signal_enabled) {
        // Replace NearestFoodPerception (3 dims) with FoodScentPerception (2 dims) + SignalPerception
        // FoodScent gives coarse direction; SignalPerception gives precise location via communication
        params.input_dim -= 3;  // remove NearestFoodPerception
        params.input_dim += FoodScentPerception::kDim;  // add coarse scent
        params.input_dim += params.signal_channels;      // add signal perception
        params.output_dim += params.signal_channels;
    }
    params.coop_enabled = config.getOr<bool>("world.coop.enabled", false);
    params.heavy_resource_count = config.getOr<std::size_t>("world.coop.heavy_count", 3);
    params.heavy_resource_amount = config.getOr<float>("world.coop.heavy_amount", 30.0f);
    params.heavy_required_workers = config.getOr<std::size_t>("world.coop.required_workers", 3);
    params.heavy_required_ticks = config.getOr<std::size_t>("world.coop.required_ticks", 5);
    params.heavy_collect_radius = config.getOr<float>("world.coop.collect_radius", 5.0f);
    params.coop_weight = config.getOr<double>("reward.coop_weight", 3.0);
    params.coop_proximity_bonus = config.getOr<double>("reward.coop_proximity_bonus", 0.5);
    params.novelty_enabled = config.getOr<bool>("reward.novelty_enabled", false);
    params.novelty_weight = config.getOr<double>("reward.novelty_weight", 5.0);
    params.novelty_k_neighbors = config.getOr<std::size_t>("reward.novelty_k_neighbors", 15);
    params.novelty_threshold = config.getOr<float>("reward.novelty_threshold", 0.3f);
    params.pred_enabled = config.getOr<bool>("world.predator.enabled", false);
    params.pred_ratio = config.getOr<float>("world.predator.ratio", 0.25f);
    params.pred_hunt_radius = config.getOr<float>("world.predator.hunt_radius", 5.0f);
    params.pred_damage = config.getOr<float>("world.predator.damage", 0.5f);
    params.pred_energy_gain = config.getOr<float>("world.predator.energy_gain", 0.3f);
    params.pred_hunt_cost = config.getOr<float>("world.predator.hunt_cost", 0.02f);
    params.pred_flee_radius = config.getOr<float>("world.predator.flee_radius", 15.0f);
    params.pred_flee_boost = config.getOr<float>("world.predator.flee_boost", 3.0f);
    params.pred_flee_cost = config.getOr<float>("world.predator.flee_cost", 0.03f);
    if (params.pred_enabled) {
        params.output_dim += 1;  // hunt output for predators
        params.output_dim += 1;  // flee output for prey
    }
    params.curriculum_enabled = config.getOr<bool>("evolution.curriculum.enabled", false);
    params.curriculum_stage1_gen = config.getOr<std::size_t>("evolution.curriculum.stage1_gen", 30);
    params.curriculum_stage2_gen = config.getOr<std::size_t>("evolution.curriculum.stage2_gen", 80);
    params.curriculum_stage3_gen = config.getOr<std::size_t>("evolution.curriculum.stage3_gen", 150);
    params.nsga2_enabled = config.getOr<bool>("evolution.nsga2.enabled", false);
    params.snapshot_interval = config.getOr<std::size_t>("evolution.snapshot.interval", 0);
    params.snapshot_label = config.getOr<std::string>("evolution.snapshot.label", "");
    params.resume_from = config.getOr<std::string>("evolution.snapshot.resume_from", "");
    params.terrain_mod_enabled = config.getOr<bool>("world.terrain_mod.enabled", false);
    params.terrain_mod_dig_rate = config.getOr<float>("world.terrain_mod.dig_rate", 2.0f);
    params.terrain_mod_build_rate = config.getOr<float>("world.terrain_mod.build_rate", 2.0f);
    params.terrain_mod_energy_cost = config.getOr<float>("world.terrain_mod.energy_cost", 0.5f);
    if (params.terrain_mod_enabled) {
        params.output_dim += 2;  // dig + build outputs
    }
    return params;
}

Vec2 randomPosition(const Vec2& world_size, std::mt19937& rng) {
    std::uniform_real_distribution<float> x_dist(0.0f, world_size.x);
    std::uniform_real_distribution<float> y_dist(0.0f, world_size.y);
    return {x_dist(rng), y_dist(rng)};
}

std::unique_ptr<IEvolutionStrategy> createEvolutionStrategy(const RuntimeParams& params) {
    if (params.brain_type == "fixed") {
        return std::make_unique<ClassicGA>(
            params.elite_ratio,
            params.tournament_size,
            params.weight_perturb_rate,
            params.weight_reset_rate,
            params.weight_perturb_sigma,
            params.random_seed + 5000);
    }

    if (params.brain_type == "neat") {
        if (params.nsga2_enabled) {
            return std::make_unique<Nsga2Evolution>(
                params.elite_ratio,
                params.tournament_size,
                params.weight_perturb_rate,
                params.weight_reset_rate,
                params.add_node_rate,
                params.add_connection_rate,
                params.toggle_connection_rate,
                params.add_modulatory_rate,
                params.weight_perturb_sigma,
                params.compatibility_threshold,
                params.c1_excess,
                params.c2_disjoint,
                params.c3_weight,
                params.tau_min,
                params.tau_max,
                params.random_seed + 5000,
                params.decision_interval_min,
                params.decision_interval_max);
        }
        return std::make_unique<NeatEvolution>(
            params.elite_ratio,
            params.tournament_size,
            params.weight_perturb_rate,
            params.weight_reset_rate,
            params.add_node_rate,
            params.add_connection_rate,
            params.toggle_connection_rate,
            params.add_modulatory_rate,
            params.weight_perturb_sigma,
            params.compatibility_threshold,
            params.c1_excess,
            params.c2_disjoint,
            params.c3_weight,
            params.tau_min,
            params.tau_max,
            params.random_seed + 5000,
            params.decision_interval_min,
            params.decision_interval_max);
    }

    throw std::runtime_error("Unsupported brain.type: " + params.brain_type);
}

std::vector<std::unique_ptr<IGenome>> createInitialPopulation(const RuntimeParams& params,
                                                              IEvolutionStrategy* evolution_strategy) {
    std::mt19937 rng(params.random_seed == 0 ? std::random_device{}() : params.random_seed);
    std::vector<std::unique_ptr<IGenome>> genomes;
    genomes.reserve(params.population_size);

    if (params.brain_type == "fixed") {
        for (std::size_t i = 0; i < params.population_size; ++i) {
            genomes.push_back(FixedTopologyGenome::random(
                GenomeId{i + 1},
                params.input_dim,
                params.hidden_dim,
                params.output_dim,
                rng));
        }

        return genomes;
    }

    if (params.brain_type == "neat") {
        auto* neat_evolution = dynamic_cast<NeatEvolution*>(evolution_strategy);
        if (neat_evolution == nullptr) {
            throw std::runtime_error("NEAT population requested without NeatEvolution strategy");
        }

        auto& tracker = neat_evolution->innovationTracker();
        tracker.beginGeneration();
        for (std::size_t i = 0; i < params.population_size; ++i) {
            genomes.push_back(NeatGenome::minimal(
                GenomeId{i + 1},
                params.input_dim,
                params.output_dim,
                tracker,
                rng,
                params.tau_min,
                params.tau_max,
                params.decision_interval_min,
                params.decision_interval_max));
        }

        return genomes;
    }

    throw std::runtime_error("Unsupported brain.type: " + params.brain_type);
}

std::unique_ptr<IBrain> createBrainForGenome(const IGenome& genome, const RuntimeParams& params) {
    if (params.brain_type == "fixed") {
        const auto* fixed_genome = dynamic_cast<const FixedTopologyGenome*>(&genome);
        if (fixed_genome == nullptr) {
            throw std::runtime_error("Population contains non-fixed-topology genome while brain.type=fixed");
        }
        return std::make_unique<FixedTopologyBrain>(*fixed_genome);
    }

    if (params.brain_type == "neat") {
        const auto* neat_genome = dynamic_cast<const NeatGenome*>(&genome);
        if (neat_genome == nullptr) {
            throw std::runtime_error("Population contains non-NEAT genome while brain.type=neat");
        }
        return std::make_unique<NeatBrain>(*neat_genome);
    }

    throw std::runtime_error("Unsupported brain.type: " + params.brain_type);
}

std::unordered_map<GenomeId, SpeciesId> assignSpeciesIfNeeded(IEvolutionStrategy* evolution_strategy,
                                                              const std::vector<IGenome*>& current_genomes) {
    std::unordered_map<GenomeId, SpeciesId> lookup;
    auto* neat_evolution = dynamic_cast<NeatEvolution*>(evolution_strategy);
    if (neat_evolution == nullptr) {
        return lookup;
    }

    for (const auto& group : neat_evolution->assignSpecies(current_genomes)) {
        for (const GenomeId genome_id : group.members) {
            lookup.emplace(genome_id, group.id);
        }
    }

    return lookup;
}

std::vector<std::unique_ptr<IRewardComponent>> createRewards(const RuntimeParams& params) {
    std::vector<std::unique_ptr<IRewardComponent>> rewards;
    rewards.push_back(std::make_unique<SurvivalReward>(params.survival_weight));
    rewards.push_back(std::make_unique<FoodReward>(params.food_weight));
    rewards.push_back(std::make_unique<IntrinsicReward>(params.intrinsic_weight));
    if (params.combat_enabled) {
        rewards.push_back(std::make_unique<CombatReward>(
            1.0,
            params.combat_attack_reward,
            params.combat_kill_bonus,
            params.combat_death_penalty));
    }
    if (params.coop_enabled) {
        rewards.push_back(std::make_unique<CoopReward>(
            params.coop_weight,
            params.coop_proximity_bonus));
    }
    if (params.novelty_enabled) {
        auto archive = std::make_shared<NoveltyArchive>(
            params.novelty_k_neighbors, params.novelty_threshold);
        rewards.push_back(std::make_unique<NoveltyReward>(
            params.novelty_weight, archive));
    }
    return rewards;
}

std::unique_ptr<World2D> createWorld(const RuntimeParams& params, Generation generation) {
    auto world = std::make_unique<World2D>(WorldId{static_cast<std::uint64_t>(generation) + 1}, params.world_size);

    const auto gen = static_cast<std::size_t>(generation);
    const bool curriculum = params.curriculum_enabled;
    const bool stage0 = !curriculum || gen < params.curriculum_stage1_gen;
    const bool stage1 = !curriculum || gen >= params.curriculum_stage1_gen;
    const bool stage2 = !curriculum || gen >= params.curriculum_stage2_gen;
    const bool stage3 = !curriculum || gen >= params.curriculum_stage3_gen;

    const std::size_t resource_count = stage0 ? params.resource_count * 2 : params.resource_count;
    const float resource_amount = stage0 ? params.resource_amount * 1.5f : params.resource_amount;

    world->addLayer(std::make_unique<ResourceLayer>(
        resource_count,
        resource_amount,
        params.resource_respawn_ticks,
        params.random_seed + generation + 1,
        params.dynamic_resources && stage1,
        params.resource_regen_rate));
    world->addLayer(std::make_unique<AgentLayer>(params.death_threshold, params.base_metabolism));
    if (params.terrain_enabled) {
        auto terrain_params = params.terrain_params;
        terrain_params.seed = params.terrain_params.seed + static_cast<std::uint32_t>(generation);
        world->addLayer(std::make_unique<TerrainLayer>(params.world_size, terrain_params));
    }
    if (params.day_night_enabled) {
        world->addLayer(std::make_unique<TimeOfDayLayer>(params.day_length_ticks));
    }
    if (params.season_enabled) {
        world->addLayer(std::make_unique<SeasonLayer>(params.ticks_per_season));
    }
    if (params.disease_enabled) {
        world->addLayer(std::make_unique<DiseaseLayer>(
            params.infection_radius,
            params.infection_prob,
            params.infection_duration,
            params.death_prob,
            params.initial_infection_rate,
            params.random_seed + 100));
    }
    if (params.signal_enabled) {
        world->addLayer(std::make_unique<SignalLayer>(
            params.world_size,
            params.signal_grid_resolution,
            params.signal_channels,
            params.signal_decay_rate,
            params.signal_diffusion_rate));
    }
    if (params.coop_enabled) {
        world->addLayer(std::make_unique<HeavyResourceLayer>(
            params.heavy_resource_count,
            params.heavy_resource_amount,
            params.heavy_required_workers,
            params.heavy_required_ticks,
            params.heavy_collect_radius,
            params.random_seed + 200));
    }
    if (params.curriculum_enabled) {
        const char* stage_name = stage0 ? "greenhouse" : stage1 ? "survival" : stage2 ? "competition" : "ecology";
        LOG_INFO("Curriculum stage: {} (gen={})", stage_name, gen);
    }

    return world;
}

void populateWorld(World2D& world,
                   const RuntimeParams& params,
                   const std::vector<std::unique_ptr<IGenome>>& population,
                   const std::unordered_map<GenomeId, SpeciesId>& species_lookup,
                   std::uint64_t& next_agent_id,
                   Generation generation) {
    auto* agent_layer = world.getLayer<AgentLayer>();
    if (agent_layer == nullptr) {
        throw std::runtime_error("AgentLayer missing from world");
    }

    std::mt19937 spawn_rng(params.random_seed == 0
        ? std::random_device{}()
        : params.random_seed + 1000 + generation);

    std::size_t agent_index = 0;
    for (const auto& genome_ptr : population) {
        auto brain = createBrainForGenome(*genome_ptr, params);
        if (brain->inputDim() != params.input_dim || brain->outputDim() != params.output_dim) {
            throw std::runtime_error("Brain dimensions do not match configured population shape");
        }

        float decision_interval = 1.0f;
        if (params.brain_type == "neat") {
            const auto* neat_genome = dynamic_cast<const NeatGenome*>(genome_ptr.get());
            if (neat_genome != nullptr) {
                decision_interval = neat_genome->decisionInterval();
            }
        }

        auto agent = std::make_unique<Agent>(
            AgentId{next_agent_id++},
            genome_ptr->id(),
            std::move(brain),
            randomPosition(params.world_size, spawn_rng),
            params.initial_energy,
            params.exploration_grid_size,
            decision_interval);
        if (const auto it = species_lookup.find(genome_ptr->id()); it != species_lookup.end()) {
            agent->setSpeciesId(it->second);
        }
        agent->addPerception(std::make_unique<SelfPerception>(params.initial_energy, params.episode_ticks));
        // When signal is enabled, replace direct food perception with coarse scent
        // Literature: "if individuals can directly perceive targets, communication won't evolve" (Rawal & Miikkulainen 2014)
        // FoodScentPerception gives rough direction; SignalPerception gives precise location
        if (!params.signal_enabled) {
            agent->addPerception(std::make_unique<NearestFoodPerception>());
        } else {
            agent->addPerception(std::make_unique<FoodScentPerception>());
        }
        if (params.terrain_enabled) {
            agent->addPerception(std::make_unique<TerrainPerception>(params.terrain_params.height_scale));
        }
        if (params.day_night_enabled) {
            agent->addPerception(std::make_unique<TimeOfDayPerception>());
        }
        if (params.season_enabled) {
            agent->addPerception(std::make_unique<SeasonPerception>());
        }
        if (params.disease_enabled) {
            agent->addPerception(std::make_unique<InfectionPerception>(params.infection_perception_radius));
        }
        if (params.signal_enabled) {
            agent->addPerception(std::make_unique<SignalPerception>(params.signal_channels, params.signal_perception_radius));
        }
        agent->addCapability(std::make_unique<MoveCapability>(
            params.max_speed,
            params.decision_cost,
            params.movement_cost_factor,
            params.exploration_grid_size));
        agent->addCapability(std::make_unique<EatCapability>(params.eat_radius));
        if (params.combat_enabled) {
            agent->addCapability(std::make_unique<AttackCapability>(
                params.attack_range,
                params.damage_factor,
                params.energy_transfer_rate,
                params.attack_cost,
                params.attack_threshold));
        }
        if (params.lifetime_learning_enabled) {
            agent->addCapability(std::make_unique<HebbianLearning>(
                params.hebbian_learning_rate));
        }
        if (params.terrain_mod_enabled) {
            agent->addCapability(std::make_unique<TerrainModifier>(
                params.terrain_mod_dig_rate, params.terrain_mod_build_rate,
                params.terrain_mod_energy_cost));
        }
        if (params.signal_enabled) {
            agent->addCapability(std::make_unique<EmitSignalCapability>(
                params.signal_channels, params.signal_emit_cost));
        }
        if (params.pred_enabled) {
            const bool is_pred = (agent_index < static_cast<std::size_t>(
                static_cast<float>(params.population_size) * params.pred_ratio));
            agent->setPredator(is_pred);
            // Both capabilities added to all agents so brain outputs are fully consumed
            // PredatorHuntCapability only affects non-predators; FleeCapability only affects non-predators
            agent->addCapability(std::make_unique<PredatorHuntCapability>(
                params.pred_hunt_radius,
                params.pred_damage,
                params.pred_energy_gain,
                params.pred_hunt_cost));
            agent->addCapability(std::make_unique<FleeCapability>(
                params.pred_flee_radius,
                params.pred_flee_boost,
                params.pred_flee_cost));
        }
        if (params.disease_enabled) {
            // Inherit immunity gain rate from genome (evolvable trait)
            const auto* neat_genome = dynamic_cast<const NeatGenome*>(genome_ptr.get());
            if (neat_genome) {
                agent->setImmunityGainRate(neat_genome->immunityGainRate());
            }
        }
        agent_layer->addAgent(std::move(agent), world);
        ++agent_index;
    }
}

void evaluatePopulation(World2D& world,
                        const std::vector<std::unique_ptr<IRewardComponent>>& rewards,
                        const RuntimeParams& params,
                        const std::atomic<bool>& stop_requested,
                        ModuleMetricsObserver& module_observer) {
    auto* agent_layer = world.getLayer<AgentLayer>();
    if (agent_layer == nullptr) {
        throw std::runtime_error("AgentLayer missing from world");
    }

    for (std::size_t tick = 0; tick < params.episode_ticks; ++tick) {
        if (stop_requested.load()) {
            break;
        }

        world.tick(params.tick_dt);
        module_observer.onTick(world);

        for (Agent* agent : agent_layer->agents()) {
            if (!agent->alive()) {
                continue;
            }

            for (const auto& reward : rewards) {
                agent->addFitness(reward->weight() * reward->evaluate(*agent, world, params.tick_dt));
            }
        }

        if (agent_layer->livingCount() == 0) {
            break;
        }
    }
}

std::vector<EvaluatedIndividual> collectEvaluated(
    World2D& world,
    const std::vector<std::unique_ptr<IGenome>>& population,
    bool nsga2_enabled = false) {
    auto* agent_layer = world.getLayer<AgentLayer>();
    if (agent_layer == nullptr) {
        throw std::runtime_error("AgentLayer missing from world");
    }

    // Build a set of genome IDs in this population for fast lookup
    std::unordered_set<GenomeId> pop_ids;
    for (const auto& genome : population) {
        pop_ids.insert(genome->id());
    }

    std::vector<EvaluatedIndividual> evaluated;
    evaluated.reserve(population.size());
    for (Agent* agent : agent_layer->agents()) {
        // Only include agents whose genome is in this population
        if (pop_ids.find(agent->genomeId()) == pop_ids.end()) continue;

        std::size_t complexity = 0;
        std::size_t modulatory = 0;
        float immunity_rate = 0.05f;
        for (const auto& genome : population) {
            if (genome->id() == agent->genomeId()) {
                complexity = genome->complexity();
                const auto* neat_genome = dynamic_cast<const NeatGenome*>(genome.get());
                if (neat_genome) {
                    for (const auto& node : neat_genome->nodes()) {
                        if (node.type == NodeGeneType::Modulatory) ++modulatory;
                    }
                    immunity_rate = neat_genome->immunityGainRate();
                }
                break;
            }
        }

        EvaluatedIndividual eval{
            agent->genomeId(),
            agent->fitness(),
            agent->speciesId(),
            complexity,
            modulatory,
            immunity_rate,
        };

        // Multi-objective for NSGA-II: [survival, foraging, exploration]
        if (nsga2_enabled) {
            const auto* concrete = dynamic_cast<const Agent*>(agent);
            double survival = static_cast<double>(agent->age());
            // Foraging: use fitness as proxy (dominated by food reward)
            double foraging = agent->fitness();
            double exploration = concrete ? static_cast<double>(concrete->visitedCellCount()) : 0.0;
            eval.objectives = {survival, foraging, exploration};
        }

        evaluated.push_back(std::move(eval));
    }
    return evaluated;
}

GenerationStats computeStats(const std::vector<EvaluatedIndividual>& evaluated) {
    if (evaluated.empty()) {
        return {0.0, 0.0, 0.0, 0, 0.0, 0.0, 0.0};
    }

    double min_fitness = std::numeric_limits<double>::max();
    double best_fitness = std::numeric_limits<double>::lowest();
    double fitness_sum = 0.0;
    double complexity_sum = 0.0;
    std::unordered_set<SpeciesId> species_ids;
    double modulatory_sum = 0.0;
    double immunity_rate_sum = 0.0;
    for (const auto& item : evaluated) {
        const double f = std::isfinite(item.fitness) ? item.fitness : 0.0;
        min_fitness = std::min(min_fitness, f);
        best_fitness = std::max(best_fitness, f);
        fitness_sum += f;
        complexity_sum += static_cast<double>(item.complexity);
        modulatory_sum += static_cast<double>(item.modulatory_nodes);
        immunity_rate_sum += static_cast<double>(item.immunity_gain_rate);
        if (item.species_id != kInvalidSpeciesId) {
            species_ids.insert(item.species_id);
        }
    }

    return {
        min_fitness,
        fitness_sum / static_cast<double>(evaluated.size()),
        best_fitness,
        species_ids.size(),
        complexity_sum / static_cast<double>(evaluated.size()),
        modulatory_sum / static_cast<double>(evaluated.size()),
        immunity_rate_sum / static_cast<double>(evaluated.size()),
    };
}

std::vector<IGenome*> rawGenomePointers(std::vector<std::unique_ptr<IGenome>>& population) {
    std::vector<IGenome*> raw;
    raw.reserve(population.size());
    for (auto& genome : population) {
        raw.push_back(genome.get());
    }
    return raw;
}

}  // namespace

int runCoevolution(const RuntimeParams& params,
                   const std::filesystem::path& run_dir,
                   const std::atomic<bool>& stop_requested) {
    // Two independent populations: predators and prey
    const std::size_t pred_count = static_cast<std::size_t>(
        static_cast<float>(params.population_size) * params.pred_ratio);
    const std::size_t prey_count = params.population_size - pred_count;

    // Separate evolution strategies with independent innovation trackers
    auto pred_evolution = std::make_unique<NeatEvolution>(
        params.elite_ratio, params.tournament_size,
        params.weight_perturb_rate, params.weight_reset_rate,
        params.add_node_rate, params.add_connection_rate,
        params.toggle_connection_rate, params.add_modulatory_rate,
        params.weight_perturb_sigma,
        params.compatibility_threshold,
        params.c1_excess, params.c2_disjoint, params.c3_weight,
        params.tau_min, params.tau_max,
        params.random_seed + 1,
        params.decision_interval_min, params.decision_interval_max);

    auto prey_evolution = std::make_unique<NeatEvolution>(
        params.elite_ratio, params.tournament_size,
        params.weight_perturb_rate, params.weight_reset_rate,
        params.add_node_rate, params.add_connection_rate,
        params.toggle_connection_rate, params.add_modulatory_rate,
        params.weight_perturb_sigma,
        params.compatibility_threshold,
        params.c1_excess, params.c2_disjoint, params.c3_weight,
        params.tau_min, params.tau_max,
        params.random_seed + 2,
        params.decision_interval_min, params.decision_interval_max);

    // Create initial populations
    std::mt19937 pred_rng(params.random_seed == 0 ? std::random_device{}() : params.random_seed + 100);
    std::mt19937 prey_rng(params.random_seed == 0 ? std::random_device{}() : params.random_seed + 200);

    pred_evolution->innovationTracker().beginGeneration();
    prey_evolution->innovationTracker().beginGeneration();

    std::vector<std::unique_ptr<IGenome>> pred_pop;
    pred_pop.reserve(pred_count);
    for (std::size_t i = 0; i < pred_count; ++i) {
        pred_pop.push_back(NeatGenome::minimal(
            GenomeId{i + 1}, params.input_dim, params.output_dim,
            pred_evolution->innovationTracker(), pred_rng,
            params.tau_min, params.tau_max,
            params.decision_interval_min, params.decision_interval_max));
    }

    std::vector<std::unique_ptr<IGenome>> prey_pop;
    prey_pop.reserve(prey_count);
    for (std::size_t i = 0; i < prey_count; ++i) {
        prey_pop.push_back(NeatGenome::minimal(
            GenomeId{pred_count + i + 1}, params.input_dim, params.output_dim,
            prey_evolution->innovationTracker(), prey_rng,
            params.tau_min, params.tau_max,
            params.decision_interval_min, params.decision_interval_max));
    }

    auto rewards = createRewards(params);
    FitnessRecorder recorder(run_dir / "fitness.csv");
    SpeciesObserver pred_species(run_dir / "species_log_pred.jsonl");
    SpeciesObserver prey_species(run_dir / "species_log_prey.jsonl");
    PerformanceObserver perf_observer(run_dir / "perf.csv");
    ModuleMetricsObserver module_observer(run_dir / "module_metrics.csv");
    HeatmapObserver heatmap_observer(64, 5, run_dir / "heatmaps");
    AdvancedStatsObserver advanced_stats(run_dir / "advanced_stats.csv");

    std::ofstream evo_csv(run_dir / "evolution.csv", std::ios::out | std::ios::trunc);
    if (!evo_csv.is_open()) return 1;
    evo_csv << "generation,pred_avg_fitness,pred_best_fitness,pred_species,prey_avg_fitness,prey_best_fitness,prey_species,avg_immunity_gain_rate\n";

    LOG_INFO("Coevolution: {} predators + {} prey, independent NEAT populations",
             pred_count, prey_count);

    std::uint64_t next_agent_id = 1;
    for (std::size_t generation = 0; generation < params.generations; ++generation) {
        if (stop_requested.load()) break;

        // Assign species independently
        auto pred_raw = rawGenomePointers(pred_pop);
        auto prey_raw = rawGenomePointers(prey_pop);
        auto pred_species_lookup = assignSpeciesIfNeeded(pred_evolution.get(), pred_raw);
        auto prey_species_lookup = assignSpeciesIfNeeded(prey_evolution.get(), prey_raw);

        perf_observer.markGenerationStart();
        auto world = createWorld(params, static_cast<Generation>(generation));
        recorder.onAttach(*world);
        perf_observer.onAttach(*world);
        module_observer.onAttach(*world);
        pred_species.onAttach(*world);
        prey_species.onAttach(*world);
        heatmap_observer.onAttach(*world);
        advanced_stats.onAttach(*world);

        world->eventBus().publish(events::Event{
            events::EventType::GenerationStarted,
            events::GenerationStarted{static_cast<Generation>(generation)},
            world->currentTick()});
        world->eventBus().flush();

        // Populate world with both populations
        auto* agent_layer = world->getLayer<AgentLayer>();
        if (agent_layer == nullptr) {
            throw std::runtime_error("AgentLayer missing from world");
        }
        std::mt19937 spawn_rng(params.random_seed == 0
            ? std::random_device{}()
            : params.random_seed + 1000 + generation);

        // Add predators first
        for (const auto& genome_ptr : pred_pop) {
            auto brain = createBrainForGenome(*genome_ptr, params);
            float decision_interval = 1.0f;
            const auto* ng = dynamic_cast<const NeatGenome*>(genome_ptr.get());
            if (ng) decision_interval = ng->decisionInterval();

            auto agent = std::make_unique<Agent>(
                AgentId{next_agent_id++}, genome_ptr->id(), std::move(brain),
                randomPosition(params.world_size, spawn_rng),
                params.initial_energy, params.exploration_grid_size, decision_interval);
            if (const auto it = pred_species_lookup.find(genome_ptr->id()); it != pred_species_lookup.end()) {
                agent->setSpeciesId(it->second);
            }
            agent->setPredator(true);
            // Add perceptions and capabilities (same as populateWorld)
            agent->addPerception(std::make_unique<SelfPerception>(params.initial_energy, params.episode_ticks));
            if (!params.signal_enabled) {
                agent->addPerception(std::make_unique<NearestFoodPerception>());
            } else {
                agent->addPerception(std::make_unique<FoodScentPerception>());
            }
            if (params.terrain_enabled) agent->addPerception(std::make_unique<TerrainPerception>(params.terrain_params.height_scale));
            if (params.day_night_enabled) agent->addPerception(std::make_unique<TimeOfDayPerception>());
            if (params.season_enabled) agent->addPerception(std::make_unique<SeasonPerception>());
            if (params.disease_enabled) agent->addPerception(std::make_unique<InfectionPerception>(params.infection_perception_radius));
            if (params.signal_enabled) agent->addPerception(std::make_unique<SignalPerception>(params.signal_channels, params.signal_perception_radius));
            agent->addCapability(std::make_unique<MoveCapability>(params.max_speed, params.decision_cost, params.movement_cost_factor, params.exploration_grid_size));
            agent->addCapability(std::make_unique<EatCapability>(params.eat_radius));
            if (params.combat_enabled) agent->addCapability(std::make_unique<AttackCapability>(params.attack_range, params.damage_factor, params.energy_transfer_rate, params.attack_cost, params.attack_threshold));
            if (params.lifetime_learning_enabled) agent->addCapability(std::make_unique<HebbianLearning>(params.hebbian_learning_rate));
            if (params.terrain_mod_enabled) agent->addCapability(std::make_unique<TerrainModifier>(params.terrain_mod_dig_rate, params.terrain_mod_build_rate, params.terrain_mod_energy_cost));
            if (params.signal_enabled) agent->addCapability(std::make_unique<EmitSignalCapability>(params.signal_channels, params.signal_emit_cost));
            agent->addCapability(std::make_unique<PredatorHuntCapability>(params.pred_hunt_radius, params.pred_damage, params.pred_energy_gain, params.pred_hunt_cost));
            agent->addCapability(std::make_unique<FleeCapability>(params.pred_flee_radius, params.pred_flee_boost, params.pred_flee_cost));
            if (params.disease_enabled && ng) agent->setImmunityGainRate(ng->immunityGainRate());
            agent_layer->addAgent(std::move(agent), *world);
        }

        // Add prey
        for (const auto& genome_ptr : prey_pop) {
            auto brain = createBrainForGenome(*genome_ptr, params);
            float decision_interval = 1.0f;
            const auto* ng = dynamic_cast<const NeatGenome*>(genome_ptr.get());
            if (ng) decision_interval = ng->decisionInterval();

            auto agent = std::make_unique<Agent>(
                AgentId{next_agent_id++}, genome_ptr->id(), std::move(brain),
                randomPosition(params.world_size, spawn_rng),
                params.initial_energy, params.exploration_grid_size, decision_interval);
            if (const auto it = prey_species_lookup.find(genome_ptr->id()); it != prey_species_lookup.end()) {
                agent->setSpeciesId(it->second);
            }
            agent->setPredator(false);
            agent->addPerception(std::make_unique<SelfPerception>(params.initial_energy, params.episode_ticks));
            if (!params.signal_enabled) {
                agent->addPerception(std::make_unique<NearestFoodPerception>());
            } else {
                agent->addPerception(std::make_unique<FoodScentPerception>());
            }
            if (params.terrain_enabled) agent->addPerception(std::make_unique<TerrainPerception>(params.terrain_params.height_scale));
            if (params.day_night_enabled) agent->addPerception(std::make_unique<TimeOfDayPerception>());
            if (params.season_enabled) agent->addPerception(std::make_unique<SeasonPerception>());
            if (params.disease_enabled) agent->addPerception(std::make_unique<InfectionPerception>(params.infection_perception_radius));
            if (params.signal_enabled) agent->addPerception(std::make_unique<SignalPerception>(params.signal_channels, params.signal_perception_radius));
            agent->addCapability(std::make_unique<MoveCapability>(params.max_speed, params.decision_cost, params.movement_cost_factor, params.exploration_grid_size));
            agent->addCapability(std::make_unique<EatCapability>(params.eat_radius));
            if (params.combat_enabled) agent->addCapability(std::make_unique<AttackCapability>(params.attack_range, params.damage_factor, params.energy_transfer_rate, params.attack_cost, params.attack_threshold));
            if (params.lifetime_learning_enabled) agent->addCapability(std::make_unique<HebbianLearning>(params.hebbian_learning_rate));
            if (params.terrain_mod_enabled) agent->addCapability(std::make_unique<TerrainModifier>(params.terrain_mod_dig_rate, params.terrain_mod_build_rate, params.terrain_mod_energy_cost));
            if (params.signal_enabled) agent->addCapability(std::make_unique<EmitSignalCapability>(params.signal_channels, params.signal_emit_cost));
            agent->addCapability(std::make_unique<PredatorHuntCapability>(params.pred_hunt_radius, params.pred_damage, params.pred_energy_gain, params.pred_hunt_cost));
            agent->addCapability(std::make_unique<FleeCapability>(params.pred_flee_radius, params.pred_flee_boost, params.pred_flee_cost));
            if (params.disease_enabled && ng) agent->setImmunityGainRate(ng->immunityGainRate());
            agent_layer->addAgent(std::move(agent), *world);
        }

        // Evaluate together in shared world
        perf_observer.markEvaluationStart();
        evaluatePopulation(*world, rewards, params, stop_requested, module_observer);
        perf_observer.markEvaluationEnd();

        // Collect evaluated separately for each population
        auto pred_evaluated = collectEvaluated(*world, pred_pop, params.nsga2_enabled);
        auto prey_evaluated = collectEvaluated(*world, prey_pop, params.nsga2_enabled);

        // Write Pareto fronts if NSGA-II enabled
        if (params.nsga2_enabled) {
            auto* pred_nsga = dynamic_cast<Nsga2Evolution*>(pred_evolution.get());
            auto* prey_nsga = dynamic_cast<Nsga2Evolution*>(prey_evolution.get());
            if (pred_nsga) pred_nsga->writeParetoFront(pred_evaluated, (run_dir / "pareto_pred.csv").string());
            if (prey_nsga) prey_nsga->writeParetoFront(prey_evaluated, (run_dir / "pareto_prey.csv").string());
        }

        // Compute stats
        auto pred_stats = computeStats(pred_evaluated);
        auto prey_stats = computeStats(prey_evaluated);

        // Lamarckian learning
        if (params.lifetime_learning_enabled && params.lamarck_mode) {
            auto* al = world->getLayer<AgentLayer>();
            if (al) {
                for (auto* agent : al->agents()) {
                    auto* nb = dynamic_cast<NeatBrain*>(&agent->brain());
                    if (!nb) continue;
                    const auto lw = nb->currentWeights();
                    // Check both populations
                    for (auto& genome : pred_pop) {
                        if (genome->id() == agent->genomeId()) {
                            auto* ng = dynamic_cast<NeatGenome*>(genome.get());
                            if (ng) ng->setWeights(lw);
                            break;
                        }
                    }
                    for (auto& genome : prey_pop) {
                        if (genome->id() == agent->genomeId()) {
                            auto* ng = dynamic_cast<NeatGenome*>(genome.get());
                            if (ng) ng->setWeights(lw);
                            break;
                        }
                    }
                }
            }
        }

        evo_csv << generation << ','
                << pred_stats.avg_fitness << ',' << pred_stats.best_fitness << ',' << pred_stats.species_count << ','
                << prey_stats.avg_fitness << ',' << prey_stats.best_fitness << ',' << prey_stats.species_count << ','
                << prey_stats.avg_immunity_gain_rate << '\n';
        evo_csv.flush();

        world->eventBus().publish(events::Event{
            events::EventType::GenerationEnded,
            events::GenerationEnded{
                static_cast<Generation>(generation),
                pred_stats.best_fitness,
                pred_stats.avg_fitness,
                pred_stats.avg_fitness},
            world->currentTick()});
        world->eventBus().flush();

        pred_species.onDetach(*world);
        prey_species.onDetach(*world);
        recorder.onDetach(*world);
        perf_observer.onDetach(*world);
        module_observer.onDetach(*world);
        heatmap_observer.onDetach(*world);
        advanced_stats.onDetach(*world);

        LOG_INFO("Gen {} coevo: pred avg={:.1f} best={:.1f} sp={} | prey avg={:.1f} best={:.1f} sp={} alive={}/{}",
                 generation, pred_stats.avg_fitness, pred_stats.best_fitness, pred_stats.species_count,
                 prey_stats.avg_fitness, prey_stats.best_fitness, prey_stats.species_count,
                 agent_layer ? agent_layer->livingCount() : 0, params.population_size);

        // Auto-snapshot
        if (params.snapshot_interval > 0 && generation > 0 && generation % params.snapshot_interval == 0) {
            const auto snap_dir = run_dir / "snapshots" / ("gen" + std::to_string(generation));
            SnapshotIO::save(snap_dir, pred_pop, pred_evolution->innovationTracker(),
                             static_cast<Generation>(generation), params.brain_type,
                             params.snapshot_label + "_pred");
            SnapshotIO::save(snap_dir / "prey", prey_pop, prey_evolution->innovationTracker(),
                             static_cast<Generation>(generation), params.brain_type,
                             params.snapshot_label + "_prey");
        }

        if (generation + 1 >= params.generations || stop_requested.load()) {
            perf_observer.finishGeneration(static_cast<Generation>(generation));
            break;
        }

        // Reproduce independently
        perf_observer.markReproductionStart();
        pred_evolution->innovationTracker().beginGeneration();
        prey_evolution->innovationTracker().beginGeneration();
        auto pred_raw2 = rawGenomePointers(pred_pop);
        auto prey_raw2 = rawGenomePointers(prey_pop);
        pred_pop = pred_evolution->reproduce(pred_evaluated, pred_raw2);
        prey_pop = prey_evolution->reproduce(prey_evaluated, prey_raw2);
        perf_observer.markReproductionEnd();
        perf_observer.finishGeneration(static_cast<Generation>(generation));
    }

    return 0;
}

HeadlessRuntime::HeadlessRuntime(config::Config config)
    : config_(std::move(config)) {}

HeadlessRuntime::~HeadlessRuntime() {
    if (running_.load()) stop();
}

void HeadlessRuntime::initialize() {
    if (initialized_) return;

    const auto runs_dir = std::filesystem::path(config_.getOr<std::string>("output.runs_dir", "data/runs"));
    run_dir_ = runs_dir / makeTimestamp();
    std::filesystem::create_directories(run_dir_);

    LOG_INFO("HeadlessRuntime: initialize (config={}, run_dir={})",
             config_.path().string(), run_dir_.string());

    initialized_ = true;
}

int HeadlessRuntime::run() {
    if (!initialized_) initialize();

    running_.store(true);
    stop_requested_.store(false);
    LOG_INFO("HeadlessRuntime: run starting");

    int rc = 0;
    try {
        const RuntimeParams params = loadParams(config_);

        // F-COEVO: when predators enabled with NEAT, use independent populations
        if (params.pred_enabled && params.brain_type == "neat") {
            rc = runCoevolution(params, run_dir_, stop_requested_);
            LOG_INFO("HeadlessRuntime: run finished");
            running_.store(false);
            return rc;
        }

        auto evolution_strategy = createEvolutionStrategy(params);
        std::vector<std::unique_ptr<IGenome>> population;
        Generation start_generation = 0;

        // Resume from snapshot if configured
        if (!params.resume_from.empty()) {
            InnovationTracker loaded_tracker;
            if (SnapshotIO::load(params.resume_from, population, loaded_tracker, start_generation)) {
                LOG_INFO("HeadlessRuntime: resumed from snapshot at gen {}", start_generation);
                // Update evolution strategy's innovation tracker
                auto* neat_evo = dynamic_cast<NeatEvolution*>(evolution_strategy.get());
                auto* nsga_evo = dynamic_cast<Nsga2Evolution*>(evolution_strategy.get());
                if (neat_evo) neat_evo->innovationTracker() = loaded_tracker;
                if (nsga_evo) nsga_evo->innovationTracker() = loaded_tracker;
            } else {
                LOG_WARN("HeadlessRuntime: failed to resume from snapshot, starting fresh");
                population = createInitialPopulation(params, evolution_strategy.get());
                start_generation = 0;
            }
        } else {
            population = createInitialPopulation(params, evolution_strategy.get());
        }
        auto rewards = createRewards(params);
        FitnessRecorder recorder(run_dir_ / "fitness.csv");
        std::unique_ptr<SpeciesObserver> species_observer;
        if (params.brain_type == "neat") {
            species_observer = std::make_unique<SpeciesObserver>(run_dir_ / "species_log.jsonl");
        }
        PerformanceObserver perf_observer(run_dir_ / "perf.csv");
        ModuleMetricsObserver module_observer(run_dir_ / "module_metrics.csv");
        HeatmapObserver heatmap_observer(64, 5, run_dir_ / "heatmaps");
        AdvancedStatsObserver advanced_stats(run_dir_ / "advanced_stats.csv");
        std::ofstream evolution_metrics(run_dir_ / "evolution.csv", std::ios::out | std::ios::trunc);
        if (!evolution_metrics.is_open()) {
            throw std::runtime_error("Failed to open evolution metrics output file");
        }
        evolution_metrics << "generation,min_fitness,avg_fitness,best_fitness,species_count,avg_complexity,avg_modulatory,avg_immunity_gain_rate\n";
        LOG_INFO("HeadlessRuntime: using evolution strategy '{}' with brain.type='{}'",
                 evolution_strategy->name(),
                 params.brain_type);

        std::uint64_t next_agent_id = 1;
        for (std::size_t generation = start_generation; generation < params.generations; ++generation) {
            if (stop_requested_.load()) {
                break;
            }

            auto current_raw = rawGenomePointers(population);
            const auto species_lookup = assignSpeciesIfNeeded(evolution_strategy.get(), current_raw);
            perf_observer.markGenerationStart();
            auto world = createWorld(params, static_cast<Generation>(generation));
            recorder.onAttach(*world);
            perf_observer.onAttach(*world);
            module_observer.onAttach(*world);
            heatmap_observer.onAttach(*world);
            advanced_stats.onAttach(*world);
            if (species_observer) {
                species_observer->onAttach(*world);
            }
            world->eventBus().publish(events::Event{
                events::EventType::GenerationStarted,
                events::GenerationStarted{static_cast<Generation>(generation)},
                world->currentTick()});
            world->eventBus().flush();

            populateWorld(*world,
                          params,
                          population,
                          species_lookup,
                          next_agent_id,
                          static_cast<Generation>(generation));
            perf_observer.markEvaluationStart();
            evaluatePopulation(*world, rewards, params, stop_requested_, module_observer);
            perf_observer.markEvaluationEnd();

            const auto evaluated = collectEvaluated(*world, population, params.nsga2_enabled);

            // Write Pareto front if NSGA-II enabled
            if (params.nsga2_enabled) {
                auto* nsga_evo = dynamic_cast<Nsga2Evolution*>(evolution_strategy.get());
                if (nsga_evo) {
                    const auto pareto_file = (run_dir_ / "pareto_front.csv").string();
                    nsga_evo->writeParetoFront(evaluated, pareto_file);
                }
            }

            if (params.lifetime_learning_enabled && params.lamarck_mode) {
                auto* agent_layer_lam = world->getLayer<AgentLayer>();
                if (agent_layer_lam != nullptr) {
                    for (auto* agent : agent_layer_lam->agents()) {
                        auto* neat_brain = dynamic_cast<NeatBrain*>(&agent->brain());
                        if (neat_brain == nullptr) continue;
                        const auto learned_weights = neat_brain->currentWeights();
                        for (auto& genome : population) {
                            if (genome->id() == agent->genomeId()) {
                                auto* neat_genome = dynamic_cast<NeatGenome*>(genome.get());
                                if (neat_genome != nullptr) {
                                    neat_genome->setWeights(learned_weights);
                                }
                                break;
                            }
                        }
                    }
                }
            }

            const auto stats = computeStats(evaluated);
            const auto* agent_layer = world->getLayer<AgentLayer>();
            evolution_metrics << generation << ','
                              << stats.min_fitness << ','
                              << stats.avg_fitness << ','
                              << stats.best_fitness << ','
                              << stats.species_count << ','
                              << stats.avg_complexity << ','
                              << stats.avg_modulatory_nodes << ','
                              << stats.avg_immunity_gain_rate << '\n';
            evolution_metrics.flush();
            world->eventBus().publish(events::Event{
                events::EventType::GenerationEnded,
                events::GenerationEnded{
                    static_cast<Generation>(generation),
                    stats.best_fitness,
                    stats.avg_fitness,
                    stats.min_fitness,
                },
                world->currentTick()});
            world->eventBus().flush();
            if (species_observer) {
                species_observer->onDetach(*world);
            }
            recorder.onDetach(*world);
            perf_observer.onDetach(*world);
            module_observer.onDetach(*world);
            heatmap_observer.onDetach(*world);
            advanced_stats.onDetach(*world);

            LOG_INFO(
                "Generation {} complete: min={:.3f} avg={:.3f} best={:.3f} species={} complexity={:.2f} alive={}/{}",
                generation,
                stats.min_fitness,
                stats.avg_fitness,
                stats.best_fitness,
                stats.species_count,
                stats.avg_complexity,
                agent_layer != nullptr ? agent_layer->livingCount() : 0,
                params.population_size);

            // Auto-snapshot
            if (params.snapshot_interval > 0 && generation > 0 && generation % params.snapshot_interval == 0) {
                const auto snap_dir = run_dir_ / "snapshots" / ("gen" + std::to_string(generation));
                InnovationTracker dummy_tracker;
                SnapshotIO::save(snap_dir, population, dummy_tracker,
                                 static_cast<Generation>(generation), params.brain_type,
                                 params.snapshot_label);
            }

            if (generation + 1 >= params.generations || stop_requested_.load()) {
                perf_observer.finishGeneration(static_cast<Generation>(generation));
                break;
            }

            perf_observer.markReproductionStart();
            auto raw = rawGenomePointers(population);
            population = evolution_strategy->reproduce(evaluated, raw);
            perf_observer.markReproductionEnd();
            perf_observer.finishGeneration(static_cast<Generation>(generation));
        }
    } catch (const std::exception& e) {
        LOG_ERROR("HeadlessRuntime failed: {}", e.what());
        rc = 1;
    }

    LOG_INFO("HeadlessRuntime: run finished");
    running_.store(false);
    return rc;
}

void HeadlessRuntime::stop() {
    stop_requested_.store(true);
    LOG_INFO("HeadlessRuntime: stop requested");
}

}  // namespace neuro::runtime
