#include "runtime/ProtagonistEcologyConfig.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace neuro::routes::protagonist {

namespace {

std::string normalizeRole(std::string role) {
    std::transform(role.begin(), role.end(), role.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (role == "prod" || role == "production") {
        return "live";
    }
    return role;
}

SimTimeSeconds getDurationCompat(const config::Config& config,
                                 const char* seconds_key,
                                 const char* legacy_ticks_key,
                                 SimTimeSeconds default_value) {
    return config.getOr<double>(seconds_key,
                                config.getOr<double>(legacy_ticks_key, default_value));
}

 float getRateCompat(const config::Config& config,
                     const char* per_second_key,
                     const char* legacy_per_tick_key,
                     float default_value) {
     return config.getOr<float>(per_second_key,
                                config.getOr<float>(legacy_per_tick_key, default_value));
 }

}  // namespace

bool ProtagonistEcologyConfig::isTrainRole() const {
    return world_role == "train";
}

bool ProtagonistEcologyConfig::isEvalRole() const {
    return world_role == "eval";
}

bool ProtagonistEcologyConfig::isLiveRole() const {
    return world_role == "live";
}

ProtagonistEcologyConfig ProtagonistEcologyConfig::fromConfig(const config::Config& config) {
    const auto world_size = config.getArray<double>("world.size");
    if (world_size.size() != 2) {
        throw std::runtime_error("world.size must have exactly 2 elements");
    }

    ProtagonistEcologyConfig out{};
    out.random_seed = static_cast<std::uint32_t>(config.getOr<int>("runtime.random_seed", 42));
    out.world_role = normalizeRole(config.getOr<std::string>("runtime.world_role", out.world_role));
    out.scenario_name = config.getOr<std::string>("runtime.scenario_name", out.scenario_name);
    if (!out.isTrainRole() && !out.isEvalRole() && !out.isLiveRole()) {
        throw std::runtime_error("runtime.world_role must be one of: train, eval, live");
    }
    out.world_size = Vec2{static_cast<float>(world_size[0]), static_cast<float>(world_size[1])};
    out.sim_time.substep_dt = config.getOr<double>("simulation.time.substep_dt", out.sim_time.substep_dt);
    out.sim_time.max_substeps_per_advance = config.getOr<std::size_t>("simulation.time.max_substeps_per_advance", out.sim_time.max_substeps_per_advance);
    out.sim_time.preview_duration_seconds = getDurationCompat(config,
                                                              "simulation.time.preview_duration_seconds",
                                                              "runtime.preview_ticks",
                                                              out.sim_time.preview_duration_seconds);
    out.compute.protagonist_dense_gpu_enabled = config.getOr<bool>("compute.protagonist_dense_gpu_enabled", out.compute.protagonist_dense_gpu_enabled);
    out.compute.protagonist_batched_dense_enabled = config.getOr<bool>("compute.protagonist_batched_dense_enabled", out.compute.protagonist_batched_dense_enabled);
    out.compute.protagonist_batched_dense_force_cpu = config.getOr<bool>("compute.protagonist_batched_dense_force_cpu", out.compute.protagonist_batched_dense_force_cpu);
    out.compute.agent_decision_threads = config.getOr<std::size_t>("compute.agent_decision_threads", out.compute.agent_decision_threads);

    out.terrain_enabled = config.getOr<bool>("world.terrain.enabled", true);
    out.terrain.height_scale = config.getOr<float>("world.terrain.height_scale", out.terrain.height_scale);
    out.terrain.slope_resistance = config.getOr<float>("world.terrain.slope_resistance", out.terrain.slope_resistance);
    out.terrain.cave_density = config.getOr<float>("world.terrain.cave_density", out.terrain.cave_density);
    out.terrain.cave_shelter_factor = config.getOr<float>("world.terrain.cave_shelter_factor", out.terrain.cave_shelter_factor);
    out.terrain.min_velocity_factor = config.getOr<float>("world.terrain.min_velocity_factor", out.terrain.min_velocity_factor);
    out.terrain.seed = static_cast<std::uint32_t>(config.getOr<int>("world.terrain.seed", static_cast<int>(out.random_seed)));
    out.terrain.grid_resolution = config.getOr<std::size_t>("world.terrain.grid_resolution", out.terrain.grid_resolution);
    out.terrain.perlin_frequency = config.getOr<float>("world.terrain.perlin_frequency", out.terrain.perlin_frequency);
    out.terrain.perlin_persistence = config.getOr<float>("world.terrain.perlin_persistence", out.terrain.perlin_persistence);
    out.terrain.perlin_octaves = config.getOr<int>("world.terrain.perlin_octaves", out.terrain.perlin_octaves);

    out.resources.count = config.getOr<std::size_t>("world.resources.count", out.resources.count);
    out.resources.amount = config.getOr<float>("world.resources.amount", out.resources.amount);
    out.resources.respawn_seconds = getDurationCompat(config,
                                                      "world.resources.respawn_seconds",
                                                      "world.resources.respawn_ticks",
                                                      out.resources.respawn_seconds);
    out.resources.dynamic = config.getOr<bool>("world.resources.dynamic", out.resources.dynamic);
    out.resources.regen_rate = config.getOr<float>("world.resources.regen_rate", out.resources.regen_rate);
    out.resources.terrain_adjustment_enabled = config.getOr<bool>("world.resources.terrain_adjustment_enabled", out.resources.terrain_adjustment_enabled);
    out.resources.plant_grass_weight = config.getOr<float>("world.resources.plant_weights.grass", out.resources.plant_grass_weight);
    out.resources.plant_browse_weight = config.getOr<float>("world.resources.plant_weights.browse", out.resources.plant_browse_weight);
    out.resources.plant_fruit_weight = config.getOr<float>("world.resources.plant_weights.fruit", out.resources.plant_fruit_weight);

    out.signal.enabled = config.getOr<bool>("world.signal.enabled", out.signal.enabled);
    out.signal.protagonist_emit_enabled = config.getOr<bool>("world.signal.protagonist_emit_enabled", out.signal.protagonist_emit_enabled);
    out.signal.channels = config.getOr<std::size_t>("world.signal.channels", out.signal.channels);
    out.signal.grid_resolution = config.getOr<std::size_t>("world.signal.grid_resolution", out.signal.grid_resolution);
    out.signal.decay_rate = config.getOr<float>("world.signal.decay_rate", out.signal.decay_rate);
    out.signal.diffusion_rate = config.getOr<float>("world.signal.diffusion_rate", out.signal.diffusion_rate);
    out.signal.controlled_emitter_enabled = config.getOr<bool>("world.signal.controlled_emitter_enabled", out.signal.controlled_emitter_enabled);
    out.signal.controlled_emitter_period_seconds = config.getOr<double>("world.signal.controlled_emitter_period_seconds", out.signal.controlled_emitter_period_seconds);
    out.signal.controlled_emitter_offset_radius = config.getOr<float>("world.signal.controlled_emitter_offset_radius", out.signal.controlled_emitter_offset_radius);
    out.signal.controlled_emitter_offset_jitter = config.getOr<float>("world.signal.controlled_emitter_offset_jitter", out.signal.controlled_emitter_offset_jitter);
    out.signal.controlled_emitter_randomize_position = config.getOr<bool>("world.signal.controlled_emitter_randomize_position", out.signal.controlled_emitter_randomize_position);
    out.signal.controlled_emitter_food_radius = config.getOr<float>("world.signal.controlled_emitter_food_radius", out.signal.controlled_emitter_food_radius);
    out.signal.controlled_emitter_food_count = config.getOr<std::size_t>("world.signal.controlled_emitter_food_count", out.signal.controlled_emitter_food_count);
    out.signal.controlled_emitter_food_amount = config.getOr<float>("world.signal.controlled_emitter_food_amount", out.signal.controlled_emitter_food_amount);
    // A3.S1 ablation string -> enum. Default "none" = SignalAblation::None.
    // Accept any case; unknown values fall back to None and silently leave a
    // log-friendly canonical name in the parsed config.
    {
        std::string ablation_str = config.getOr<std::string>("world.signal.ablation", std::string{"none"});
        std::transform(ablation_str.begin(), ablation_str.end(), ablation_str.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ablation_str == "disabled") {
            out.signal.ablation = SignalAblation::Disabled;
        } else if (ablation_str == "listener_blind") {
            out.signal.ablation = SignalAblation::ListenerBlind;
        } else if (ablation_str == "one_channel_only") {
            out.signal.ablation = SignalAblation::OneChannelOnly;
        } else if (ablation_str == "noise") {
            out.signal.ablation = SignalAblation::Noise;
        } else {
            out.signal.ablation = SignalAblation::None;
        }
    }
    out.signal.noise_position_stddev = config.getOr<float>("world.signal.noise_position_stddev", out.signal.noise_position_stddev);

    out.objects.enabled = config.getOr<bool>("world.objects.enabled", out.objects.enabled);
    out.objects.count = config.getOr<std::size_t>("world.objects.count", out.objects.count);
    out.objects.damping = config.getOr<float>("world.objects.damping", out.objects.damping);

    out.base.enabled = config.getOr<bool>("world.base.enabled", out.base.enabled);
    out.base.center.x = config.getOr<float>("world.base.center_x", out.base.center.x);
    out.base.center.y = config.getOr<float>("world.base.center_y", out.base.center.y);
    out.base.radius = config.getOr<float>("world.base.radius", out.base.radius);
    out.base.stockpile_radius = config.getOr<float>("world.base.stockpile_radius", out.base.stockpile_radius);
    out.base.nest_radius = config.getOr<float>("world.base.nest_radius", out.base.nest_radius);
    out.base.stockpile_offset = config.getOr<float>("world.base.stockpile_offset", out.base.stockpile_offset);
    out.base.nest_offset = config.getOr<float>("world.base.nest_offset", out.base.nest_offset);

    out.nursery.enabled = config.getOr<bool>("world.nursery.enabled", out.nursery.enabled);
    out.nursery.deposit_energy_bonus = config.getOr<float>("world.nursery.deposit_energy_bonus", out.nursery.deposit_energy_bonus);
    out.nursery.deposit_fitness_bonus = config.getOr<float>("world.nursery.deposit_fitness_bonus", out.nursery.deposit_fitness_bonus);
    out.nursery.stockpile_snap_radius = config.getOr<float>("world.nursery.stockpile_snap_radius", out.nursery.stockpile_snap_radius);

    out.nursery_supply.enabled = config.getOr<bool>("world.nursery_supply.enabled", out.nursery_supply.enabled);
    out.nursery_supply.min_nearby_sticks = config.getOr<std::size_t>("world.nursery_supply.min_nearby_sticks", out.nursery_supply.min_nearby_sticks);
    out.nursery_supply.replenish_batch = config.getOr<std::size_t>("world.nursery_supply.replenish_batch", out.nursery_supply.replenish_batch);
    out.nursery_supply.near_radius = config.getOr<float>("world.nursery_supply.near_radius", out.nursery_supply.near_radius);
    out.nursery_supply.spawn_radius = config.getOr<float>("world.nursery_supply.spawn_radius", out.nursery_supply.spawn_radius);
    out.nursery_supply.cooldown_seconds = getDurationCompat(config,
                                                            "world.nursery_supply.cooldown_seconds",
                                                            "world.nursery_supply.cooldown_ticks",
                                                            out.nursery_supply.cooldown_seconds);

    out.campfire.enabled = config.getOr<bool>("world.campfire.enabled", out.campfire.enabled);
    out.campfire.count = config.getOr<std::size_t>("world.campfire.count", out.campfire.count);
    out.campfire.spawn_radius = config.getOr<float>("world.campfire.spawn_radius", out.campfire.spawn_radius);
    out.campfire.warmth_radius = config.getOr<float>("world.campfire.warmth_radius", out.campfire.warmth_radius);
    out.campfire.danger_radius = config.getOr<float>("world.campfire.danger_radius", out.campfire.danger_radius);
    out.campfire.warmth_energy_bonus = config.getOr<float>("world.campfire.warmth_energy_bonus", out.campfire.warmth_energy_bonus);
    out.campfire.danger_damage = config.getOr<float>("world.campfire.danger_damage", out.campfire.danger_damage);

    out.water.enabled = config.getOr<bool>("world.water.enabled", out.water.enabled);
    out.water.source_count = config.getOr<std::size_t>("world.water.source_count", out.water.source_count);
    out.water.spawn_radius_min = config.getOr<float>("world.water.spawn_radius_min", out.water.spawn_radius_min);
    out.water.spawn_radius_max = config.getOr<float>("world.water.spawn_radius_max", out.water.spawn_radius_max);
    out.water.source_radius = config.getOr<float>("world.water.source_radius", out.water.source_radius);
    out.water.drink_radius = config.getOr<float>("world.water.drink_radius", out.water.drink_radius);
    out.water.drink_rate = config.getOr<float>("world.water.drink_rate", out.water.drink_rate);
    out.water.direction_requires_visibility = config.getOr<bool>("world.water.direction_requires_visibility", out.water.direction_requires_visibility);

    out.survival.enabled = config.getOr<bool>("world.survival.enabled", out.survival.enabled);
    out.survival.max_health = config.getOr<float>("world.survival.max_health", out.survival.max_health);
    out.survival.max_hydration = config.getOr<float>("world.survival.max_hydration", out.survival.max_hydration);
    out.survival.initial_health_ratio = config.getOr<float>("world.survival.initial_health_ratio", out.survival.initial_health_ratio);
    out.survival.initial_hydration_ratio = config.getOr<float>("world.survival.initial_hydration_ratio", out.survival.initial_hydration_ratio);
    out.survival.hydration_decay_per_second = getRateCompat(config,
                                                            "world.survival.hydration_decay_per_second",
                                                            "world.survival.hydration_decay_per_tick",
                                                            out.survival.hydration_decay_per_second);
    out.survival.dehydration_threshold_ratio = config.getOr<float>("world.survival.dehydration_threshold_ratio", out.survival.dehydration_threshold_ratio);
    out.survival.dehydration_damage_per_second = getRateCompat(config,
                                                               "world.survival.dehydration_damage_per_second",
                                                               "world.survival.dehydration_damage_per_tick",
                                                               out.survival.dehydration_damage_per_second);
    out.survival.fire_health_damage_per_second = getRateCompat(config,
                                                               "world.survival.fire_health_damage_per_second",
                                                               "world.survival.fire_health_damage_per_tick",
                                                               out.survival.fire_health_damage_per_second);
    out.survival.warmth_recovery_per_second = getRateCompat(config,
                                                            "world.survival.warmth_recovery_per_second",
                                                            "world.survival.warmth_recovery_per_tick",
                                                            out.survival.warmth_recovery_per_second);
    out.survival.nest_recovery_per_second = getRateCompat(config,
                                                          "world.survival.nest_recovery_per_second",
                                                          "world.survival.nest_recovery_per_tick",
                                                          out.survival.nest_recovery_per_second);
    out.survival.injured_threshold_ratio = config.getOr<float>("world.survival.injured_threshold_ratio", out.survival.injured_threshold_ratio);

    out.tree.enabled = config.getOr<bool>("world.tree.enabled", out.tree.enabled);
    out.tree.count = config.getOr<std::size_t>("world.tree.count", out.tree.count);
    out.tree.spawn_radius_min = config.getOr<float>("world.tree.spawn_radius_min", out.tree.spawn_radius_min);
    out.tree.spawn_radius_max = config.getOr<float>("world.tree.spawn_radius_max", out.tree.spawn_radius_max);
    out.tree.trunk_radius = config.getOr<float>("world.tree.trunk_radius", out.tree.trunk_radius);
    out.tree.max_integrity = config.getOr<float>("world.tree.max_integrity", out.tree.max_integrity);
    out.tree.wood_yield = config.getOr<std::size_t>("world.tree.wood_yield", out.tree.wood_yield);
    out.tree.regrow_seconds = getDurationCompat(config,
                                                "world.tree.regrow_seconds",
                                                "world.tree.regrow_ticks",
                                                out.tree.regrow_seconds);

    out.worksite.enabled = config.getOr<bool>("world.worksite.enabled", out.worksite.enabled);
    out.worksite.count = config.getOr<std::size_t>("world.worksite.count", out.worksite.count);
    out.worksite.spawn_radius = config.getOr<float>("world.worksite.spawn_radius", out.worksite.spawn_radius);
    out.worksite.interaction_radius = config.getOr<float>("world.worksite.interaction_radius", out.worksite.interaction_radius);
    out.worksite.required_sticks = config.getOr<std::size_t>("world.worksite.required_sticks", out.worksite.required_sticks);
    out.worksite.required_stones = config.getOr<std::size_t>("world.worksite.required_stones", out.worksite.required_stones);
    out.worksite.assembly_rate = config.getOr<float>("world.worksite.assembly_rate", out.worksite.assembly_rate);
    out.worksite.shelter_energy_bonus = config.getOr<float>("world.worksite.shelter_energy_bonus", out.worksite.shelter_energy_bonus);
    out.worksite.direction_requires_visibility = config.getOr<bool>("world.worksite.direction_requires_visibility", out.worksite.direction_requires_visibility);
    out.worksite.preactivate = config.getOr<bool>("world.worksite.preactivate", out.worksite.preactivate);

    out.intervention.enabled = config.getOr<bool>("world.intervention.enabled", out.intervention.enabled);
    out.intervention.initial_budget = config.getOr<float>("world.intervention.initial_budget", out.intervention.initial_budget);
    out.intervention.max_budget = config.getOr<float>("world.intervention.max_budget", out.intervention.max_budget);
    out.intervention.recharge_per_second = getRateCompat(config,
                                                         "world.intervention.recharge_per_second",
                                                         "world.intervention.recharge_per_tick",
                                                         out.intervention.recharge_per_second);
    out.intervention.stress_decay_per_second = getRateCompat(config,
                                                             "world.intervention.stress_decay_per_second",
                                                             "world.intervention.stress_decay_per_tick",
                                                             out.intervention.stress_decay_per_second);
    out.intervention.max_stress = config.getOr<float>("world.intervention.max_stress", out.intervention.max_stress);
    out.intervention.cooldown_seconds = getDurationCompat(config,
                                                          "world.intervention.cooldown_seconds",
                                                          "world.intervention.cooldown_ticks",
                                                          out.intervention.cooldown_seconds);

    out.progression.enabled = config.getOr<bool>("world.progression.enabled", out.progression.enabled);
    out.progression.first_fire_stick_threshold = config.getOr<std::size_t>("world.progression.first_fire_stick_threshold", out.progression.first_fire_stick_threshold);
    out.progression.worksite_unlock_deposit_threshold = config.getOr<std::size_t>("world.progression.worksite_unlock_deposit_threshold", out.progression.worksite_unlock_deposit_threshold);
    out.progression.worksite_unlock_stone_threshold = config.getOr<std::size_t>("world.progression.worksite_unlock_stone_threshold", out.progression.worksite_unlock_stone_threshold);
    out.progression.stone_unlock_deposit_threshold = config.getOr<std::size_t>("world.progression.stone_unlock_deposit_threshold", out.progression.stone_unlock_deposit_threshold);
    out.progression.heavy_unlock_deposit_threshold = config.getOr<std::size_t>("world.progression.heavy_unlock_deposit_threshold", out.progression.heavy_unlock_deposit_threshold);
    out.progression.second_fire_deposit_threshold = config.getOr<std::size_t>("world.progression.second_fire_deposit_threshold", out.progression.second_fire_deposit_threshold);
    out.progression.heavy_unlock_stone_threshold = config.getOr<std::size_t>("world.progression.heavy_unlock_stone_threshold", out.progression.heavy_unlock_stone_threshold);
    out.progression.second_fire_stone_threshold = config.getOr<std::size_t>("world.progression.second_fire_stone_threshold", out.progression.second_fire_stone_threshold);
    out.progression.stone_unlock_count = config.getOr<std::size_t>("world.progression.stone_unlock_count", out.progression.stone_unlock_count);
    out.progression.heavy_unlock_count = config.getOr<std::size_t>("world.progression.heavy_unlock_count", out.progression.heavy_unlock_count);
    out.progression.unlock_radius_min = config.getOr<float>("world.progression.unlock_radius_min", out.progression.unlock_radius_min);
    out.progression.unlock_radius_max = config.getOr<float>("world.progression.unlock_radius_max", out.progression.unlock_radius_max);
    out.progression.event_min_interval_seconds = getDurationCompat(config,
                                                                  "world.progression.event_min_interval_seconds",
                                                                  "world.progression.event_min_interval_ticks",
                                                                  out.progression.event_min_interval_seconds);
    out.progression.event_max_interval_seconds = getDurationCompat(config,
                                                                  "world.progression.event_max_interval_seconds",
                                                                  "world.progression.event_max_interval_ticks",
                                                                  out.progression.event_max_interval_seconds);

    out.day_night.enabled = config.getOr<bool>("world.day_night.enabled", out.day_night.enabled);
    out.day_night.day_length_seconds = getDurationCompat(config,
                                                         "world.day_night.day_length_seconds",
                                                         "world.day_night.day_length_ticks",
                                                         out.day_night.day_length_seconds);
    out.day_night.initial_time = config.getOr<float>("world.day_night.initial_time", out.day_night.initial_time);

    out.season.enabled = config.getOr<bool>("world.season.enabled", out.season.enabled);
    out.season.season_length_seconds = getDurationCompat(config,
                                                         "world.season.season_length_seconds",
                                                         "world.season.ticks_per_season",
                                                         out.season.season_length_seconds);
    out.season.start_time_seconds = getDurationCompat(config,
                                                      "world.season.start_time_seconds",
                                                      "world.season.start_tick",
                                                      out.season.start_time_seconds);

    out.disease.enabled = config.getOr<bool>("world.disease.enabled", out.disease.enabled);
    out.disease.infection_radius = config.getOr<float>("world.disease.infection_radius", out.disease.infection_radius);
    out.disease.infection_prob = config.getOr<float>("world.disease.infection_prob", out.disease.infection_prob);
    out.disease.infection_duration = config.getOr<float>("world.disease.infection_duration", out.disease.infection_duration);
    out.disease.death_prob = config.getOr<float>("world.disease.death_prob", out.disease.death_prob);
    out.disease.initial_infection_rate = config.getOr<float>("world.disease.initial_infection_rate", out.disease.initial_infection_rate);

    out.memory_trial.enabled = config.getOr<bool>("world.memory_trial.enabled", out.memory_trial.enabled);
    out.memory_trial.cue_duration_seconds = config.getOr<double>("world.memory_trial.cue_duration_seconds", out.memory_trial.cue_duration_seconds);
    out.memory_trial.cue_radius = config.getOr<float>("world.memory_trial.cue_radius", out.memory_trial.cue_radius);
    out.memory_trial.choice_deadline_seconds = config.getOr<double>("world.memory_trial.choice_deadline_seconds", out.memory_trial.choice_deadline_seconds);
    out.memory_trial.target_radius = config.getOr<float>("world.memory_trial.target_radius", out.memory_trial.target_radius);
    out.memory_trial.success_fitness_bonus = config.getOr<float>("world.memory_trial.success_fitness_bonus", out.memory_trial.success_fitness_bonus);
    out.memory_trial.failure_fitness_penalty = config.getOr<float>("world.memory_trial.failure_fitness_penalty", out.memory_trial.failure_fitness_penalty);

    out.protagonist.count = config.getOr<std::size_t>("population.protagonist.count", out.protagonist.count);
    out.protagonist.hidden_dim = config.getOr<std::size_t>("population.protagonist.hidden_dim", out.protagonist.hidden_dim);
    out.protagonist.memory_cells = config.getOr<std::size_t>("population.protagonist.memory_cells", out.protagonist.memory_cells);
    out.protagonist.memory_slots = config.getOr<std::size_t>("population.protagonist.memory_slots", out.protagonist.memory_slots);
    out.protagonist.read_heads = config.getOr<std::size_t>("population.protagonist.read_heads", out.protagonist.read_heads);
    out.protagonist.action_dim = config.getOr<std::size_t>("population.protagonist.action_dim", out.protagonist.action_dim);
    out.protagonist.memory_cognition_enabled = config.getOr<bool>("population.protagonist.memory_cognition_enabled", out.protagonist.memory_cognition_enabled);
    out.protagonist.spatial_memory_writes_enabled = config.getOr<bool>("population.protagonist.spatial_memory_writes_enabled", out.protagonist.spatial_memory_writes_enabled);
    out.protagonist.episodic_memory_writes_enabled = config.getOr<bool>("population.protagonist.episodic_memory_writes_enabled", out.protagonist.episodic_memory_writes_enabled);
    out.protagonist.social_memory_writes_enabled = config.getOr<bool>("population.protagonist.social_memory_writes_enabled", out.protagonist.social_memory_writes_enabled);
    out.protagonist.goal_manager_enabled = config.getOr<bool>("population.protagonist.goal_manager_enabled", out.protagonist.goal_manager_enabled);
    out.protagonist.goal_action_assist_enabled = config.getOr<bool>("population.protagonist.goal_action_assist_enabled", out.protagonist.goal_action_assist_enabled);
    out.protagonist.predator_warn_enabled = config.getOr<bool>("population.protagonist.predator_warn_enabled", out.protagonist.predator_warn_enabled);
    out.protagonist.hunt_enabled = config.getOr<bool>("population.protagonist.hunt_enabled", out.protagonist.hunt_enabled);

    out.background.forager_count = config.getOr<std::size_t>("population.background.forager_count", out.background.forager_count);
    out.background.predator_count = config.getOr<std::size_t>("population.background.predator_count", out.background.predator_count);
    out.background.ambient_count = config.getOr<std::size_t>("population.background.ambient_count", out.background.ambient_count);
    out.background.hidden_dim = config.getOr<std::size_t>("population.background.hidden_dim", out.background.hidden_dim);

    if (auto species_view = config.raw().at_path("population.background.species"); species_view && species_view.is_array()) {
        out.background.slots.clear();
        if (const auto* arr = species_view.as_array()) {
            for (const auto& element : *arr) {
                const auto* slot_table = element.as_table();
                if (slot_table == nullptr) continue;
                SpeciesSlotConfig slot{};
                if (const auto& archetype = (*slot_table)["archetype"]; archetype.is_string()) {
                    slot.archetype = archetype.value<std::string>().value_or("large_herbivore");
                }
                if (const auto& count = (*slot_table)["count"]; count.is_integer()) {
                    slot.count = static_cast<std::size_t>(count.value<std::int64_t>().value_or(0));
                }
                if (slot.count > 0) {
                    out.background.slots.push_back(std::move(slot));
                }
            }
        }
    }

    // L2.4 v2 task templates: parse [[evolution.task_templates]] array.
    if (auto tt_view = config.raw().at_path("evolution.task_templates"); tt_view && tt_view.is_array()) {
        if (const auto* arr = tt_view.as_array()) {
            for (const auto& element : *arr) {
                const auto* tt = element.as_table();
                if (tt == nullptr) continue;
                TaskTemplate tpl{};
                if (const auto& name = (*tt)["name"]; name.is_string()) {
                    tpl.name = name.value<std::string>().value_or("unnamed");
                }
                if (const auto& w = (*tt)["weight"]; w.is_number()) {
                    tpl.weight = static_cast<float>(w.value<double>().value_or(1.0));
                }
                if (const auto& d = (*tt)["difficulty"]; d.is_number()) {
                    tpl.difficulty = static_cast<float>(d.value<double>().value_or(1.0));
                }
                if (const auto& v = (*tt)["protagonist_count"]; v.is_integer()) {
                    tpl.protagonist_count = static_cast<std::size_t>(v.value<std::int64_t>().value_or(0));
                    tpl.has_protagonist_count = true;
                }
                if (const auto& v = (*tt)["worksite_count"]; v.is_integer()) {
                    tpl.worksite_count = static_cast<std::size_t>(v.value<std::int64_t>().value_or(0));
                    tpl.has_worksite_count = true;
                }
                if (const auto& v = (*tt)["tree_count"]; v.is_integer()) {
                    tpl.tree_count = static_cast<std::size_t>(v.value<std::int64_t>().value_or(0));
                    tpl.has_tree_count = true;
                }
                if (const auto& v = (*tt)["drive_weight"]; v.is_number()) {
                    tpl.drive_weight = v.value<double>().value_or(0.0);
                    tpl.has_drive_weight = true;
                }
                if (const auto& v = (*tt)["outcome_weight"]; v.is_number()) {
                    tpl.outcome_weight = v.value<double>().value_or(1.0);
                    tpl.has_outcome_weight = true;
                }
                if (const auto& v = (*tt)["worksite_completion_reward"]; v.is_number()) {
                    tpl.worksite_completion_reward = static_cast<float>(v.value<double>().value_or(0.0));
                    tpl.has_worksite_completion_reward = true;
                }
                if (const auto& v = (*tt)["shelter_camp_reward"]; v.is_number()) {
                    tpl.shelter_camp_reward = static_cast<float>(v.value<double>().value_or(0.0));
                    tpl.has_shelter_camp_reward = true;
                }
                if (const auto& v = (*tt)["worksite_preactivate"]; v.is_boolean()) {
                    tpl.worksite_preactivate = v.value<bool>().value_or(false);
                    tpl.has_worksite_preactivate = true;
                }
                // background_overrides is itself an array of {archetype, count} tables.
                if (const auto& bg = (*tt)["background"]; bg.is_array()) {
                    if (const auto* bg_arr = bg.as_array()) {
                        for (const auto& bg_el : *bg_arr) {
                            const auto* bg_tab = bg_el.as_table();
                            if (bg_tab == nullptr) continue;
                            TaskTemplateOverride ov{};
                            if (const auto& a = (*bg_tab)["archetype"]; a.is_string()) {
                                ov.archetype = a.value<std::string>().value_or("");
                            }
                            if (const auto& c = (*bg_tab)["count"]; c.is_integer()) {
                                ov.count = static_cast<std::size_t>(c.value<std::int64_t>().value_or(0));
                            }
                            if (!ov.archetype.empty()) {
                                tpl.background_overrides.push_back(std::move(ov));
                            }
                        }
                    }
                }
                if (tpl.weight > 0.0f) {
                    out.task_templates.push_back(std::move(tpl));
                }
            }
        }
    }
    // L2.6 v11: stable curriculum toggle (default true).
    out.task_template_stable_within_generation = config.getOr<bool>(
        "evolution.task_template_stable_within_generation",
        out.task_template_stable_within_generation);
    // L2.6 v12: context-conditioned policy toggle (default true).
    out.task_context_perception_enabled = config.getOr<bool>(
        "evolution.task_context_perception_enabled",
        out.task_context_perception_enabled);
    // L2.6 v12: ACCEL regret-based dynamic weight (default true).
    out.task_template_dynamic_weight = config.getOr<bool>(
        "evolution.task_template_dynamic_weight",
        out.task_template_dynamic_weight);
    // L2.6 v12: hard-tasks-first warm-up window (default 20 generations).
    out.task_template_hard_first_generations = config.getOr<std::size_t>(
        "evolution.task_template_hard_first_generations",
        out.task_template_hard_first_generations);

    return out;
}

}  // namespace neuro::routes::protagonist
