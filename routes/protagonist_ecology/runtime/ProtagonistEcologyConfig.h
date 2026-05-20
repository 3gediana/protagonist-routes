#pragma once

#include "core/config/Config.h"
#include "core/types/Aliases.h"
#include "core/types/Vec2.h"
#include "core/world/layers/terrain/TerrainGenerator.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace neuro::routes::protagonist {

struct ResourceConfig {
    std::size_t count{80};
    float amount{18.0f};
    SimTimeSeconds respawn_seconds{15.0};
    bool dynamic{true};
    float regen_rate{0.2f};
    bool terrain_adjustment_enabled{true};
};

struct SignalConfig {
    bool enabled{true};
    std::size_t channels{2};
    std::size_t grid_resolution{64};
    float decay_rate{0.05f};
    float diffusion_rate{0.08f};
};

struct ObjectWorldConfig {
    bool enabled{true};
    std::size_t count{48};
    float damping{0.9f};
};

struct BaseZoneConfig {
    bool enabled{true};
    Vec2 center{60.0f, 100.0f};
    float radius{18.0f};
    float stockpile_radius{8.0f};
    float nest_radius{10.0f};
    float stockpile_offset{14.0f};
    float nest_offset{-8.0f};
};

struct NurseryLogisticsConfig {
    bool enabled{true};
    float deposit_energy_bonus{8.0f};
    float deposit_fitness_bonus{3.0f};
    float stockpile_snap_radius{6.0f};
};

struct NurserySupplyConfig {
    bool enabled{true};
    std::size_t min_nearby_sticks{8};
    std::size_t replenish_batch{4};
    float near_radius{16.0f};
    float spawn_radius{10.0f};
    SimTimeSeconds cooldown_seconds{25.0};
};

struct CampfireConfig {
    bool enabled{true};
    std::size_t count{2};
    float spawn_radius{16.0f};
    float warmth_radius{11.0f};
    float danger_radius{3.5f};
    float warmth_energy_bonus{0.05f};
    float danger_damage{0.4f};
};

struct WaterConfig {
    bool enabled{true};
    std::size_t source_count{2};
    float spawn_radius_min{20.0f};
    float spawn_radius_max{42.0f};
    float source_radius{5.0f};
    float drink_radius{3.0f};
    float drink_rate{24.0f};
};

struct SurvivalConfig {
    bool enabled{true};
    float max_health{100.0f};
    float max_hydration{100.0f};
    float initial_health_ratio{1.0f};
    float initial_hydration_ratio{0.72f};
    float hydration_decay_per_tick{0.055f};
    float dehydration_threshold_ratio{0.3f};
    float dehydration_damage_per_tick{0.18f};
    float fire_health_damage_per_tick{0.45f};
    float warmth_recovery_per_tick{0.06f};
    float nest_recovery_per_tick{0.1f};
    float injured_threshold_ratio{0.45f};
};

struct TreeConfig {
    bool enabled{true};
    std::size_t count{10};
    float spawn_radius_min{18.0f};
    float spawn_radius_max{40.0f};
    float trunk_radius{2.4f};
    float max_integrity{16.0f};
    std::size_t wood_yield{3};
    SimTimeSeconds regrow_seconds{240.0};
};

struct WorksiteConfig {
    bool enabled{true};
    std::size_t count{2};
    float spawn_radius{22.0f};
    float interaction_radius{6.0f};
    std::size_t required_sticks{4};
    std::size_t required_stones{2};
    float assembly_rate{0.08f};
    float shelter_energy_bonus{0.03f};
};

struct InterventionConfig {
    bool enabled{true};
    float initial_budget{1.4f};
    float max_budget{3.0f};
    float recharge_per_tick{0.025f};
    float stress_decay_per_tick{0.01f};
    float max_stress{0.85f};
    SimTimeSeconds cooldown_seconds{18.0};
};

struct DayNightConfig {
    bool enabled{true};
    SimTimeSeconds day_length_seconds{800.0};
    float initial_time{0.0f};
};

struct SeasonConfig {
    bool enabled{true};
    SimTimeSeconds season_length_seconds{2400.0};
    SimTimeSeconds start_time_seconds{0.0};
};

struct DiseaseConfig {
    bool enabled{true};
    float infection_radius{4.0f};
    float infection_prob{0.012f};
    float infection_duration{260.0f};
    float death_prob{0.03f};
    float initial_infection_rate{0.04f};
};

struct ProgressionConfig {
    bool enabled{true};
    std::size_t first_fire_stick_threshold{4};
    std::size_t worksite_unlock_deposit_threshold{8};
    std::size_t worksite_unlock_stone_threshold{1};
    std::size_t stone_unlock_deposit_threshold{6};
    std::size_t heavy_unlock_deposit_threshold{12};
    std::size_t second_fire_deposit_threshold{16};
    std::size_t heavy_unlock_stone_threshold{2};
    std::size_t second_fire_stone_threshold{3};
    std::size_t stone_unlock_count{10};
    std::size_t heavy_unlock_count{6};
    float unlock_radius_min{24.0f};
    float unlock_radius_max{42.0f};
    SimTimeSeconds event_min_interval_seconds{20.0};
    SimTimeSeconds event_max_interval_seconds{80.0};
};

struct SimulationTimeConfig {
    SimTimeSeconds substep_dt{0.1};
    std::size_t max_substeps_per_advance{16};
    SimTimeSeconds preview_duration_seconds{1.0};
};

struct ProtagonistBrainConfig {
    std::size_t count{32};
    std::size_t hidden_dim{512};
    std::size_t memory_cells{48};
    std::size_t action_dim{13};
};

struct SpeciesSlotConfig {
    std::string archetype{"large_herbivore"};
    std::size_t count{0};
};

struct BackgroundBrainConfig {
    std::size_t forager_count{0};
    std::size_t predator_count{0};
    std::size_t ambient_count{0};
    std::size_t hidden_dim{32};
    std::vector<SpeciesSlotConfig> slots{};
};

struct ProtagonistEcologyConfig {
    std::uint32_t random_seed{42};
    std::string world_role{"train"};
    std::string scenario_name{"bootstrap"};
    Vec2 world_size{200.0f, 200.0f};
    SimulationTimeConfig sim_time{};
    bool terrain_enabled{true};
    TerrainParams terrain{};
    ResourceConfig resources{};
    SignalConfig signal{};
    ObjectWorldConfig objects{};
    BaseZoneConfig base{};
    NurseryLogisticsConfig nursery{};
    NurserySupplyConfig nursery_supply{};
    CampfireConfig campfire{};
    WaterConfig water{};
    SurvivalConfig survival{};
    TreeConfig tree{};
    WorksiteConfig worksite{};
    InterventionConfig intervention{};
    ProgressionConfig progression{};
    DayNightConfig day_night{};
    SeasonConfig season{};
    DiseaseConfig disease{};
    ProtagonistBrainConfig protagonist{};
    BackgroundBrainConfig background{};

    bool isTrainRole() const;
    bool isEvalRole() const;
    bool isLiveRole() const;

    static ProtagonistEcologyConfig fromConfig(const config::Config& config);
};

}  // namespace neuro::routes::protagonist
