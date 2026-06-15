#include "runtime/ProtagonistEcologyConfig.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>

#include "core/logging/Logger.h"

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

// Layer 6 R1 (FINAL_BLUEPRINT.md S6): three-world placeholder accessors.
// "borderland" and "main" currently fall through to live-mode behavior
// (single persistent world). The full TripleWorldRuntime + MigrationScheduler
// is deferred per LAYER6_THREE_WORLDS_SPEC.md.
bool ProtagonistEcologyConfig::isBorderlandRole() const {
    return world_role == "borderland";
}

bool ProtagonistEcologyConfig::isMainWorldRole() const {
    return world_role == "main" || world_role == "main_world";
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
    if (!out.isTrainRole() && !out.isEvalRole() && !out.isLiveRole()
        && !out.isBorderlandRole() && !out.isMainWorldRole()) {
        throw std::runtime_error("runtime.world_role must be one of: train, eval, live, borderland, main");
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
    // Realism Phase B (2026-05-30): tunable environmental survival pressure
    // (cold / heat / hunger). Defaults match the prior hardcoded ctor values.
    out.survival.hunger_damage_threshold = config.getOr<float>("world.survival.hunger_damage_threshold", out.survival.hunger_damage_threshold);
    out.survival.hunger_damage_per_second = config.getOr<float>("world.survival.hunger_damage_per_second", out.survival.hunger_damage_per_second);
    out.survival.cold_temp_threshold = config.getOr<float>("world.survival.cold_temp_threshold", out.survival.cold_temp_threshold);
    out.survival.hot_temp_threshold = config.getOr<float>("world.survival.hot_temp_threshold", out.survival.hot_temp_threshold);
    out.survival.thermal_damage_per_second = config.getOr<float>("world.survival.thermal_damage_per_second", out.survival.thermal_damage_per_second);

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
    out.worksite.type_dist_wall    = config.getOr<float>("world.worksite.type_dist_wall",    out.worksite.type_dist_wall);
    out.worksite.type_dist_shelter = config.getOr<float>("world.worksite.type_dist_shelter", out.worksite.type_dist_shelter);
    out.worksite.type_dist_storage = config.getOr<float>("world.worksite.type_dist_storage", out.worksite.type_dist_storage);
    out.worksite.type_dist_lookout = config.getOr<float>("world.worksite.type_dist_lookout", out.worksite.type_dist_lookout);

    out.craft.require_in_base = config.getOr<bool>("world.craft.require_in_base", out.craft.require_in_base);
    out.craft.require_stone = config.getOr<bool>("world.craft.require_stone", out.craft.require_stone);
    out.craft.respect_worksite_reservation = config.getOr<bool>("world.craft.respect_worksite_reservation", out.craft.respect_worksite_reservation);
    out.craft.activation_threshold = config.getOr<float>("world.craft.activation_threshold", out.craft.activation_threshold);
    out.craft.energy_cost = config.getOr<float>("world.craft.energy_cost", out.craft.energy_cost);

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

    // Layer 2 (FINAL_BLUEPRINT): weather system.
    out.weather.enabled = config.getOr<bool>("world.weather.enabled", out.weather.enabled);
    out.weather.seed = static_cast<std::uint32_t>(config.getOr<int>("world.weather.seed", static_cast<int>(out.weather.seed)));
    out.weather.average_duration_seconds = config.getOr<double>("world.weather.average_duration_seconds", out.weather.average_duration_seconds);
    out.weather.min_duration_seconds = config.getOr<double>("world.weather.min_duration_seconds", out.weather.min_duration_seconds);
    out.weather.max_duration_seconds = config.getOr<double>("world.weather.max_duration_seconds", out.weather.max_duration_seconds);
    out.weather.base_prob_clear    = config.getOr<float>("world.weather.base_prob_clear",    out.weather.base_prob_clear);
    out.weather.base_prob_cloudy   = config.getOr<float>("world.weather.base_prob_cloudy",   out.weather.base_prob_cloudy);
    out.weather.base_prob_rain     = config.getOr<float>("world.weather.base_prob_rain",     out.weather.base_prob_rain);
    out.weather.base_prob_storm    = config.getOr<float>("world.weather.base_prob_storm",    out.weather.base_prob_storm);
    out.weather.base_prob_highwind = config.getOr<float>("world.weather.base_prob_highwind", out.weather.base_prob_highwind);
    out.weather.base_prob_typhoon  = config.getOr<float>("world.weather.base_prob_typhoon",  out.weather.base_prob_typhoon);
    out.weather.summer_storm_mult   = config.getOr<float>("world.weather.summer_storm_mult",   out.weather.summer_storm_mult);
    out.weather.summer_typhoon_mult = config.getOr<float>("world.weather.summer_typhoon_mult", out.weather.summer_typhoon_mult);
    out.weather.winter_typhoon_mult = config.getOr<float>("world.weather.winter_typhoon_mult", out.weather.winter_typhoon_mult);
    out.weather.winter_rain_mult    = config.getOr<float>("world.weather.winter_rain_mult",    out.weather.winter_rain_mult);

    out.disease.enabled = config.getOr<bool>("world.disease.enabled", out.disease.enabled);
    out.disease.infection_radius = config.getOr<float>("world.disease.infection_radius", out.disease.infection_radius);
    out.disease.infection_prob = config.getOr<float>("world.disease.infection_prob", out.disease.infection_prob);
    out.disease.infection_duration = config.getOr<float>("world.disease.infection_duration", out.disease.infection_duration);
    out.disease.death_prob = config.getOr<float>("world.disease.death_prob", out.disease.death_prob);
    out.disease.initial_infection_rate = config.getOr<float>("world.disease.initial_infection_rate", out.disease.initial_infection_rate);

    // Layer 4 (FINAL_BLUEPRINT.md): population / lifecycle / reproduction.
    out.population.enabled = config.getOr<bool>("world.population.enabled", out.population.enabled);
    out.population.reproduction_check_interval = config.getOr<double>("world.population.reproduction_check_interval", out.population.reproduction_check_interval);
    out.population.reproduction_radius = config.getOr<float>("world.population.reproduction_radius", out.population.reproduction_radius);
    out.population.reproduction_min_energy = config.getOr<float>("world.population.reproduction_min_energy", out.population.reproduction_min_energy);
    out.population.reproduction_energy_cost = config.getOr<float>("world.population.reproduction_energy_cost", out.population.reproduction_energy_cost);
    out.population.juvenile_initial_energy = config.getOr<float>("world.population.juvenile_initial_energy", out.population.juvenile_initial_energy);
    out.population.reproduction_cooldown_sec = config.getOr<float>("world.population.reproduction_cooldown_sec", out.population.reproduction_cooldown_sec);
    out.population.juvenile_initial_cooldown_sec = config.getOr<float>("world.population.juvenile_initial_cooldown_sec", out.population.juvenile_initial_cooldown_sec);
    out.population.elder_extra_mortality_per_sec = config.getOr<float>("world.population.elder_extra_mortality_per_sec", out.population.elder_extra_mortality_per_sec);
    out.population.max_living_agents = static_cast<std::size_t>(config.getOr<int>("world.population.max_living_agents", static_cast<int>(out.population.max_living_agents)));
    out.population.seed = static_cast<std::uint32_t>(config.getOr<int>("world.population.seed", static_cast<int>(out.population.seed)));
    out.population.max_spawns_per_tick = static_cast<std::size_t>(config.getOr<int>("world.population.max_spawns_per_tick", static_cast<int>(out.population.max_spawns_per_tick)));
    out.population.max_age_protagonist     = config.getOr<double>("world.population.max_age_protagonist",     out.population.max_age_protagonist);
    out.population.max_age_large_herbivore = config.getOr<double>("world.population.max_age_large_herbivore", out.population.max_age_large_herbivore);
    out.population.max_age_small_forager   = config.getOr<double>("world.population.max_age_small_forager",   out.population.max_age_small_forager);
    out.population.max_age_apex_predator   = config.getOr<double>("world.population.max_age_apex_predator",   out.population.max_age_apex_predator);
    out.population.max_age_pack_hunter     = config.getOr<double>("world.population.max_age_pack_hunter",     out.population.max_age_pack_hunter);
    out.population.max_age_ambush_predator = config.getOr<double>("world.population.max_age_ambush_predator", out.population.max_age_ambush_predator);
    out.population.max_age_scavenger       = config.getOr<double>("world.population.max_age_scavenger",       out.population.max_age_scavenger);
    out.population.max_age_omnivore        = config.getOr<double>("world.population.max_age_omnivore",        out.population.max_age_omnivore);
    out.population.max_age_rabbit          = config.getOr<double>("world.population.max_age_rabbit",          out.population.max_age_rabbit);

    // Layer 8 R1: Lamarckian.
    out.lamarckian.enabled = config.getOr<bool>("evolution.lamarckian.enabled", out.lamarckian.enabled);
    out.lamarckian.learning_rate = config.getOr<float>("evolution.lamarckian.learning_rate", out.lamarckian.learning_rate);
    out.lamarckian.reward_threshold = config.getOr<float>("evolution.lamarckian.reward_threshold", out.lamarckian.reward_threshold);

    // L6 R2 stage 2: TripleWorldRuntime in-process integration.
    out.triple_world.enabled = config.getOr<bool>("runtime.triple_world.enabled", out.triple_world.enabled);
    out.triple_world.top_k_protagonist_per_gen = config.getOr<std::size_t>("runtime.triple_world.top_k_protagonist_per_gen", out.triple_world.top_k_protagonist_per_gen);
    out.triple_world.top_k_background_per_gen = config.getOr<std::size_t>("runtime.triple_world.top_k_background_per_gen", out.triple_world.top_k_background_per_gen);
    out.triple_world.borderland_to_main_period_gens = config.getOr<std::size_t>("runtime.triple_world.borderland_to_main_period_gens", out.triple_world.borderland_to_main_period_gens);
    out.triple_world.borderland_to_main_top_k_protagonist = config.getOr<std::size_t>("runtime.triple_world.borderland_to_main_top_k_protagonist", out.triple_world.borderland_to_main_top_k_protagonist);
    out.triple_world.borderland_to_main_top_k_background = config.getOr<std::size_t>("runtime.triple_world.borderland_to_main_top_k_background", out.triple_world.borderland_to_main_top_k_background);
    out.triple_world.borderland_max_pool_per_archetype = config.getOr<std::size_t>("runtime.triple_world.borderland_max_pool_per_archetype", out.triple_world.borderland_max_pool_per_archetype);
    out.triple_world.persist_dir = config.getOr<std::string>("runtime.triple_world.persist_dir", out.triple_world.persist_dir);
    out.triple_world.extinction_rescue_top_k = config.getOr<std::size_t>("runtime.triple_world.extinction_rescue_top_k", out.triple_world.extinction_rescue_top_k);
    out.triple_world.borderland_eval_period_gens = config.getOr<std::size_t>("runtime.triple_world.borderland_eval_period_gens", out.triple_world.borderland_eval_period_gens);
    out.triple_world.borderland_eval_aging_factor = config.getOr<double>("runtime.triple_world.borderland_eval_aging_factor", out.triple_world.borderland_eval_aging_factor);
    out.triple_world.borderland_min_continuity_gens = config.getOr<std::size_t>("runtime.triple_world.borderland_min_continuity_gens", out.triple_world.borderland_min_continuity_gens);
    out.triple_world.borderland_mutate_sigma = config.getOr<double>("runtime.triple_world.borderland_mutate_sigma", out.triple_world.borderland_mutate_sigma);
    out.triple_world.borderland_mutate_perturb_rate = config.getOr<double>("runtime.triple_world.borderland_mutate_perturb_rate", out.triple_world.borderland_mutate_perturb_rate);
    out.triple_world.borderland_mutate_reset_rate = config.getOr<double>("runtime.triple_world.borderland_mutate_reset_rate", out.triple_world.borderland_mutate_reset_rate);
    out.triple_world.borderland_mutate_add_conn_rate = config.getOr<double>("runtime.triple_world.borderland_mutate_add_conn_rate", out.triple_world.borderland_mutate_add_conn_rate);
    out.triple_world.borderland_mutate_add_node_rate = config.getOr<double>("runtime.triple_world.borderland_mutate_add_node_rate", out.triple_world.borderland_mutate_add_node_rate);
    out.triple_world.main_world_continuous_tick_enabled = config.getOr<bool>("runtime.triple_world.main_world_continuous_tick_enabled", out.triple_world.main_world_continuous_tick_enabled);
    out.triple_world.main_world_tick_dt_seconds = config.getOr<double>("runtime.triple_world.main_world_tick_dt_seconds", out.triple_world.main_world_tick_dt_seconds);
    out.triple_world.main_world_ticks_per_generation = config.getOr<std::size_t>("runtime.triple_world.main_world_ticks_per_generation", out.triple_world.main_world_ticks_per_generation);
    out.curriculum.archetype_progression_persistence_enabled = config.getOr<bool>(
        "runtime.curriculum.archetype_progression_persistence_enabled",
        out.curriculum.archetype_progression_persistence_enabled);

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
    out.protagonist.hunt_radius = config.getOr<float>("population.protagonist.hunt_radius", out.protagonist.hunt_radius);
    out.protagonist.hunt_damage = config.getOr<float>("population.protagonist.hunt_damage", out.protagonist.hunt_damage);
    out.protagonist.hunt_energy_gain = config.getOr<float>("population.protagonist.hunt_energy_gain", out.protagonist.hunt_energy_gain);
    out.protagonist.hunt_cost = config.getOr<float>("population.protagonist.hunt_cost", out.protagonist.hunt_cost);
    out.protagonist.chop_radius = config.getOr<float>("population.protagonist.chop_radius", out.protagonist.chop_radius);
    out.protagonist.chop_damage = config.getOr<float>("population.protagonist.chop_damage", out.protagonist.chop_damage);
    out.protagonist.chop_energy_cost = config.getOr<float>("population.protagonist.chop_energy_cost", out.protagonist.chop_energy_cost);
    out.protagonist.chop_progress_reward = config.getOr<float>("population.protagonist.chop_progress_reward", out.protagonist.chop_progress_reward);
    out.protagonist.building_type_choice_enabled = config.getOr<bool>("population.protagonist.building_type_choice_enabled", out.protagonist.building_type_choice_enabled);

    out.background.forager_count = config.getOr<std::size_t>("population.background.forager_count", out.background.forager_count);
    out.background.predator_count = config.getOr<std::size_t>("population.background.predator_count", out.background.predator_count);
    out.background.ambient_count = config.getOr<std::size_t>("population.background.ambient_count", out.background.ambient_count);
    out.background.hidden_dim = config.getOr<std::size_t>("population.background.hidden_dim", out.background.hidden_dim);
    out.background.dnc_memory.enabled = config.getOr<bool>("population.background.dnc_memory.enabled", out.background.dnc_memory.enabled);
    out.background.dnc_memory.slots = config.getOr<std::size_t>("population.background.dnc_memory.slots", out.background.dnc_memory.slots);
    out.background.dnc_memory.word_size = config.getOr<std::size_t>("population.background.dnc_memory.word_size", out.background.dnc_memory.word_size);
    out.background.dnc_memory.read_heads = config.getOr<std::size_t>("population.background.dnc_memory.read_heads", out.background.dnc_memory.read_heads);

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
                if (const auto& v = (*tt)["must_build_to_survive"]; v.is_boolean()) {
                    tpl.must_build_to_survive = v.value<bool>().value_or(false);
                    tpl.has_must_build_to_survive = true;
                }
                if (const auto& v = (*tt)["must_build_penalty"]; v.is_number()) {
                    tpl.must_build_penalty = static_cast<float>(v.value<double>().value_or(200.0));
                    tpl.has_must_build_penalty = true;
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
    // L2.7 v13: HRL goal-context perception toggle (default true).
    out.goal_context_perception_enabled = config.getOr<bool>(
        "evolution.goal_context_perception_enabled",
        out.goal_context_perception_enabled);
    // Realism (2026-05-30): protagonist environmental perception toggle
    // (default false -> existing lineages unchanged; true -> fresh lineage).
    out.protagonist_environment_perception_enabled = config.getOr<bool>(
        "evolution.protagonist_environment_perception_enabled",
        out.protagonist_environment_perception_enabled);
    // Candidate-1 (decision_round_9): rhythm perception toggle (cyclic day/year
    // phase + body-temp trend). Default false -> existing lineages unchanged;
    // true -> fresh lineage with +5 input dims.
    out.protagonist_rhythm_perception_enabled = config.getOr<bool>(
        "evolution.protagonist_rhythm_perception_enabled",
        out.protagonist_rhythm_perception_enabled);
    // Candidate-2 (decision_round_10): true -> fresh lineage with +2 warmth-drive dims.
    out.protagonist_warmth_drive_enabled = config.getOr<bool>(
        "evolution.protagonist_warmth_drive_enabled",
        out.protagonist_warmth_drive_enabled);
    // Border proprioception (2026-06-10 Devin): true -> fresh lineage with
    // +4 border-perception dims.
    out.protagonist_border_perception_enabled = config.getOr<bool>(
        "evolution.protagonist_border_perception_enabled",
        out.protagonist_border_perception_enabled);
    // Realism (2026-06): true -> fresh lineage with +4 cave-perception dims.
    out.protagonist_cave_perception_enabled = config.getOr<bool>(
        "evolution.protagonist_cave_perception_enabled",
        out.protagonist_cave_perception_enabled);
    // One-pot unified bloodline (2026-06-01): master switch reserves all A–F
    // dims (+11) so input_dim is locked once; Phase C FOV gating + anneal
    // schedule. current_generation is stamped by the runtime, not parsed.
    out.protagonist_unified_encoding_enabled = config.getOr<bool>(
        "evolution.protagonist_unified_encoding_enabled",
        out.protagonist_unified_encoding_enabled);
    out.protagonist_fov_enabled = config.getOr<bool>(
        "evolution.protagonist_fov_enabled", out.protagonist_fov_enabled);
    out.fov_anneal_start_gen = config.getOr<double>(
        "evolution.fov_anneal_start_gen", out.fov_anneal_start_gen);
    out.fov_anneal_end_gen = config.getOr<double>(
        "evolution.fov_anneal_end_gen", out.fov_anneal_end_gen);
    out.fov_start_half_angle_deg = config.getOr<double>(
        "evolution.fov_start_half_angle_deg", out.fov_start_half_angle_deg);
    out.fov_end_half_angle_deg = config.getOr<double>(
        "evolution.fov_end_half_angle_deg", out.fov_end_half_angle_deg);
    out.fov_occlusion_enabled = config.getOr<bool>(
        "evolution.fov_occlusion_enabled", out.fov_occlusion_enabled);
    // L2.7 v13: per-goal-type sub-reward weights. Defaults reflect the
    // relative difficulty of each goal: ChopTree/GatherStick/Stone are
    // common and easy (small reward); BuildWorksite is the long
    // chain's payoff (highest); CraftWeapon/Hunt/DrinkWater/FindFood
    // sit in between. Explore/Flee/RespondSignal/EmitSignal/RestNearFire
    // get 0 because they don't have a clean "completion" event we can
    // count today (kept as toml-tunable knobs for future extension).
    // The v12 outcome_weight still scales legacy worksite_completion_reward
    // separately; these HRL sub-rewards are independent and ride on
    // top, decoupled from outcome_weight on purpose (Cuccu/Schmidhuber
    // 2011 NeuroEvolution-style intrinsic-style dense signal that
    // doesn't get squashed when outcome_weight is small).
    constexpr std::array<float, 16> kDefaultGoalRewards = {
        0.0f,   // Explore (0)
        3.0f,   // FindWater (1)
        2.0f,   // FindFood (2)
        1.0f,   // GatherStick (3)
        1.0f,   // GatherStone (4)
        4.0f,   // ChopTree (5)
        20.0f,  // BuildWorksite (6)
        8.0f,   // CraftWeapon (7)
        10.0f,  // Hunt (8)
        0.0f,   // Flee (9)
        0.0f,   // RespondSignal (10)
        0.0f,   // EmitSignal (11)
        0.0f,   // RestNearFire (12)
        0.0f,   // ThrowAttempt (13) — default off; set negative via toml to penalise
        0.0f,   // reserved (14)
        0.0f,   // reserved (15)
    };
    out.goal_completion_rewards = kDefaultGoalRewards;
    out.goal_completion_rewards[0]  = config.getOr<float>("reward.goal_completion.explore",         out.goal_completion_rewards[0]);
    out.goal_completion_rewards[1]  = config.getOr<float>("reward.goal_completion.find_water",      out.goal_completion_rewards[1]);
    out.goal_completion_rewards[2]  = config.getOr<float>("reward.goal_completion.find_food",       out.goal_completion_rewards[2]);
    out.goal_completion_rewards[3]  = config.getOr<float>("reward.goal_completion.gather_stick",    out.goal_completion_rewards[3]);
    out.goal_completion_rewards[4]  = config.getOr<float>("reward.goal_completion.gather_stone",    out.goal_completion_rewards[4]);
    out.goal_completion_rewards[5]  = config.getOr<float>("reward.goal_completion.chop_tree",       out.goal_completion_rewards[5]);
    out.goal_completion_rewards[6]  = config.getOr<float>("reward.goal_completion.build_worksite",  out.goal_completion_rewards[6]);
    out.goal_completion_rewards[7]  = config.getOr<float>("reward.goal_completion.craft_weapon",    out.goal_completion_rewards[7]);
    out.goal_completion_rewards[8]  = config.getOr<float>("reward.goal_completion.hunt",            out.goal_completion_rewards[8]);
    out.goal_completion_rewards[9]  = config.getOr<float>("reward.goal_completion.flee",            out.goal_completion_rewards[9]);
    out.goal_completion_rewards[10] = config.getOr<float>("reward.goal_completion.respond_signal",  out.goal_completion_rewards[10]);
    out.goal_completion_rewards[11] = config.getOr<float>("reward.goal_completion.emit_signal",     out.goal_completion_rewards[11]);
    out.goal_completion_rewards[12] = config.getOr<float>("reward.goal_completion.rest_near_fire",  out.goal_completion_rewards[12]);
    out.goal_completion_rewards[13] = config.getOr<float>("reward.goal_completion.throw_attempt",   out.goal_completion_rewards[13]);
    // L2.6 v12: ACCEL regret-based dynamic weight (default true).
    out.task_template_dynamic_weight = config.getOr<bool>(
        "evolution.task_template_dynamic_weight",
        out.task_template_dynamic_weight);
    // L2.6 v12: hard-tasks-first warm-up window (default 20 generations).
    out.task_template_hard_first_generations = config.getOr<std::size_t>(
        "evolution.task_template_hard_first_generations",
        out.task_template_hard_first_generations);

    // L2.8 v14 ME-NEAT (MAP-Elites + NEAT) configuration. Toml block
    // [evolution.me_neat] enabled=true switches breed selection to
    // quality-diversity-shaped fitness. See Config.h for rationale.
    out.me_neat_enabled = config.getOr<bool>(
        "evolution.me_neat.enabled",
        out.me_neat_enabled);
    out.me_neat_chops_bin_size = config.getOr<int>(
        "evolution.me_neat.chops_bin_size",
        out.me_neat_chops_bin_size);
    out.me_neat_workcraft_bin_size = config.getOr<int>(
        "evolution.me_neat.workcraft_bin_size",
        out.me_neat_workcraft_bin_size);
    out.me_neat_max_bins = config.getOr<int>(
        "evolution.me_neat.max_bins",
        out.me_neat_max_bins);
    // v17 P2: optional 3rd BD axis (hunt count); default 0/1 = 2D backward-compat.
    out.me_neat_hunt_bin_size = config.getOr<int>(
        "evolution.me_neat.hunt_bin_size",
        out.me_neat_hunt_bin_size);
    out.me_neat_hunt_max_bins = config.getOr<int>(
        "evolution.me_neat.hunt_max_bins",
        out.me_neat_hunt_max_bins);
    // v18 CVT-MAP-Elites: opt-in replacement for grid mode. When
    // enabled, ignores hunt_bin_size / hunt_max_bins / chops_bin_size
    // grid layout - just uses bin_size * max_bins as the [0, max] range
    // per axis for normalisation.
    out.me_neat_cvt_enabled = config.getOr<bool>(
        "evolution.me_neat.cvt_enabled",
        out.me_neat_cvt_enabled);
    out.me_neat_cvt_num_centroids = config.getOr<int>(
        "evolution.me_neat.cvt_num_centroids",
        out.me_neat_cvt_num_centroids);
    out.me_neat_cvt_samples_per_centroid = config.getOr<int>(
        "evolution.me_neat.cvt_samples_per_centroid",
        out.me_neat_cvt_samples_per_centroid);
    out.me_neat_cvt_kmeans_iters = config.getOr<int>(
        "evolution.me_neat.cvt_kmeans_iters",
        out.me_neat_cvt_kmeans_iters);
    out.me_neat_lifetime_archive_enabled = config.getOr<bool>(
        "evolution.me_neat.lifetime_archive_enabled",
        out.me_neat_lifetime_archive_enabled);
    out.me_neat_non_elite_weight = config.getOr<double>(
        "evolution.me_neat.non_elite_weight",
        out.me_neat_non_elite_weight);

    // L2.9 v15: explicit archive-driven breeding (see Config.h).
    out.me_neat_archive_breeding = config.getOr<bool>(
        "evolution.me_neat.archive_breeding",
        out.me_neat_archive_breeding);
    out.me_neat_init_generations = config.getOr<std::size_t>(
        "evolution.me_neat.init_generations",
        out.me_neat_init_generations);
    out.me_neat_line_sigma = config.getOr<double>(
        "evolution.me_neat.line_sigma",
        out.me_neat_line_sigma);
    out.me_neat_mutation_boost = config.getOr<double>(
        "evolution.me_neat.mutation_boost",
        out.me_neat_mutation_boost);

    // Density auto-scaling: parse toggle and reference size.
    out.density_auto_scale = config.getOr<bool>("world.density_auto_scale", out.density_auto_scale);
    try {
        const auto ref_sz = config.getArray<double>("world.density_reference_size");
        if (ref_sz.size() == 2) {
            out.density_reference_size = Vec2{static_cast<float>(ref_sz[0]),
                                              static_cast<float>(ref_sz[1])};
        }
    } catch (...) {
        // Key not present — keep default 600×600.
    }
    if (out.density_auto_scale) {
        out.applyDensityScaling();
    }

    return out;
}

// ─── Density auto-scaling implementation ────────────────────────────
void ProtagonistEcologyConfig::applyDensityScaling() {
    const float ref_area = density_reference_size.x * density_reference_size.y;
    const float act_area = world_size.x * world_size.y;
    if (ref_area <= 0.0f || act_area <= 0.0f) return;

    const float area_ratio = act_area / ref_area;
    const float dist_ratio = std::sqrt(area_ratio);

    // Helper: scale a count, minimum 1.
    auto scaleCount = [&](std::size_t base) -> std::size_t {
        const auto v = static_cast<std::size_t>(std::round(base * area_ratio));
        return v < 1 ? 1 : v;
    };
    auto scaleDist = [&](float base) -> float {
        return base * dist_ratio;
    };

    // Resources
    resources.count = scaleCount(resources.count);

    // Trees
    tree.count = scaleCount(tree.count);
    tree.spawn_radius_min = scaleDist(tree.spawn_radius_min);
    tree.spawn_radius_max = scaleDist(tree.spawn_radius_max);

    // Campfires
    campfire.count = scaleCount(campfire.count);
    campfire.spawn_radius = scaleDist(campfire.spawn_radius);

    // Water sources
    water.source_count = scaleCount(water.source_count);
    water.spawn_radius_min = scaleDist(water.spawn_radius_min);
    water.spawn_radius_max = scaleDist(water.spawn_radius_max);

    // Worksites
    worksite.count = scaleCount(worksite.count);
    worksite.spawn_radius = scaleDist(worksite.spawn_radius);

    // Objects (sticks, stones, etc.)
    objects.count = scaleCount(objects.count);

    // Base zone radius + offsets
    base.radius = scaleDist(base.radius);
    base.stockpile_radius = scaleDist(base.stockpile_radius);
    base.nest_radius = scaleDist(base.nest_radius);
    base.center.x = scaleDist(base.center.x);
    base.center.y = scaleDist(base.center.y);

    // Background species counts (via task_templates and direct species list
    // are not scaled here — they're parsed from TOML as absolute values.
    // The Agent should scale background species counts explicitly, or
    // a separate mechanism handles them in BootstrapPopulation).
    //
    // NOTE: protagonist.count is NOT scaled — NEAT population size is
    // independent of world area (selection pressure vs spatial density
    // are separate concerns).

    LOG_INFO("density_auto_scale: ref={}x{} actual={}x{} area_ratio={:.3f} dist_ratio={:.3f}"
             " | trees={} resources={} campfires={} water={} worksites={} objects={}",
             density_reference_size.x, density_reference_size.y,
             world_size.x, world_size.y,
             area_ratio, dist_ratio,
             tree.count, resources.count, campfire.count,
             water.source_count, worksite.count, objects.count);
}

}  // namespace neuro::routes::protagonist
