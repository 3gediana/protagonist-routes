#pragma once

#include "core/config/Config.h"
#include "core/types/Aliases.h"
#include "core/types/Vec2.h"
#include "core/world/layers/terrain/TerrainGenerator.h"

#include <cstddef>
#include <cstdint>
#include <array>
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
    // L2.3 v2 (SPECIES_DIFFERENTIATION_SPEC.md § 3.1): plant cell kind
    // spawn weights. Defaults to grass=1.0 / browse=0 / fruit=0 so old
    // tomls (and v9 training) behave exactly like before. Tomls opting
    // into niche partitioning set the 3 weights explicitly under
    // `[world.resources.plant_weights]`.
    float plant_grass_weight{1.0f};
    float plant_browse_weight{0.0f};
    float plant_fruit_weight{0.0f};
};

// A3.S1 communication ablation knobs. Drives the four probes against
// the signal-response stack:
//   None             - default, no ablation
//   Disabled         - emit events are dropped at the runtime layer (no
//                      AgentEmittedSignal published, controlled emitter
//                      skipped). The strongest probe: should drive every
//                      downstream signal metric to 0.
//   ListenerBlind    - emit events are still published, but listener block
//                      treats last_signal_emit as empty (no dense reward,
//                      no halo entry, no peer_signal_heard episodic, no
//                      mask bits in CommunicationMetrics). Isolates
//                      "signal exists" from "listener uses it".
//   OneChannelOnly   - all emit events are forced onto channel 0 at the
//                      subscriber layer. Probes whether multi-channel
//                      semantics are doing real work.
//   Noise            - emit position is perturbed by gaussian noise with
//                      stddev `noise_position_stddev` before reaching
//                      the listener block. Probes whether listeners
//                      depend on precise position vs coarse direction.
enum class SignalAblation : std::uint8_t {
    None = 0,
    Disabled = 1,
    ListenerBlind = 2,
    OneChannelOnly = 3,
    Noise = 4,
};

struct SignalConfig {
    bool enabled{true};
    bool protagonist_emit_enabled{true};
    std::size_t channels{2};
    std::size_t grid_resolution{64};
    float decay_rate{0.05f};
    float diffusion_rate{0.08f};
    // Controlled emitter (was hard-coded to memory_ablation_signal_response eval).
    // When enabled, the runtime spawns periodic pulses from a fixed/randomised
    // location and (optionally) drops transient food at the emitter site so
    // an agent that learns to follow the signal gets a real reward signal.
    bool controlled_emitter_enabled{false};
    SimTimeSeconds controlled_emitter_period_seconds{30.0};
    float controlled_emitter_offset_radius{18.0f};        // distance from base.center
    float controlled_emitter_offset_jitter{8.0f};         // random episode-level jitter on offset radius
    bool controlled_emitter_randomize_position{false};    // if true, pick a fresh angle each episode
    float controlled_emitter_food_radius{6.0f};           // food drop scatter radius at emitter
    std::size_t controlled_emitter_food_count{2};
    float controlled_emitter_food_amount{15.0f};
    // A3.S1 ablation.
    SignalAblation ablation{SignalAblation::None};
    float noise_position_stddev{1.5f};  // meters; used only when ablation == Noise
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
    // (Removed 2026-05-22) `nest_predator_damage_per_second` was a
    // temporary stand-in that gave the nest a magical "predators die on
    // entry" rule so comm_danger_warning eval could produce non-zero
    // survived deltas before the physical shelter mechanism existed. It
    // is gone now: shelter physics comes from completed worksites
    // (WorksiteLayer::enforcePredatorObstacles), as designed in
    // FINAL_STATE_VISION § 3.7 / § 4.6 (emergent shelter L1).
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
    bool direction_requires_visibility{false};
};

struct SurvivalConfig {
    bool enabled{true};
    float max_health{100.0f};
    float max_hydration{100.0f};
    float initial_health_ratio{1.0f};
    float initial_hydration_ratio{0.72f};
    float hydration_decay_per_second{0.055f};
    float dehydration_threshold_ratio{0.3f};
    float dehydration_damage_per_second{0.18f};
    float fire_health_damage_per_second{0.45f};
    float warmth_recovery_per_second{0.06f};
    float nest_recovery_per_second{0.1f};
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
    bool direction_requires_visibility{false};
    // When true the world factory activates the first dormant worksite at
    // bootstrap regardless of world_role. Used by eval tasks that need a
    // ready-to-build site without flipping the whole role to "live" (which
    // would also re-seed objects with extra stones, ignite a campfire, etc).
    bool preactivate{false};
};

struct InterventionConfig {
    bool enabled{true};
    float initial_budget{1.4f};
    float max_budget{3.0f};
    float recharge_per_second{0.025f};
    float stress_decay_per_second{0.01f};
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

struct MemoryTrialConfig {
    bool enabled{false};
    SimTimeSeconds cue_duration_seconds{2.0};
    float cue_radius{10.0f};
    SimTimeSeconds choice_deadline_seconds{18.0};
    float target_radius{8.0f};
    float success_fitness_bonus{24.0f};
    float failure_fitness_penalty{6.0f};
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

struct ComputeConfig {
    bool protagonist_dense_gpu_enabled{true};
    // G1: route protagonist agents through a batched dense forward
    // (single CUDA call, or CPU reference path when CUDA not available).
    // Disabled by default until validated.
    bool protagonist_batched_dense_enabled{false};
    // Force CPU even when CUDA is ready. Used by tests / debugging.
    bool protagonist_batched_dense_force_cpu{false};
    std::size_t agent_decision_threads{0};
};

struct ProtagonistBrainConfig {
    std::size_t count{32};
    std::size_t hidden_dim{512};
    std::size_t memory_cells{48};
    std::size_t memory_slots{64};
    std::size_t read_heads{4};
    std::size_t action_dim{13};
    bool memory_cognition_enabled{true};
    bool spatial_memory_writes_enabled{true};
    bool episodic_memory_writes_enabled{true};
    bool social_memory_writes_enabled{true};
    bool goal_manager_enabled{true};
    bool goal_action_assist_enabled{false};
    bool predator_warn_enabled{true};
    bool hunt_enabled{true};
};

struct SpeciesSlotConfig {
    std::string archetype{"large_herbivore"};
    std::size_t count{0};
};

// L2.4 v2 (SPECIES_DIFFERENTIATION_SPEC.md § 4 autocurriculum). A
// per-episode "scenario" override. Each generation samples a template
// (weighted) and applies its overrides on top of the base toml before
// the episode bootstraps. ACCEL-style regret-based weighting (spec
// § 4.1) is deferred to L2.5 - this v0 uses static weights only.
struct TaskTemplateOverride {
    std::string archetype;
    std::size_t count{0};
};
struct TaskTemplate {
    std::string name{"default"};
    float weight{1.0f};
    // L2.6 v12: declared difficulty (1.0 = easy, 2.0 = hard, etc.).
    // Used by the Hard-Tasks-First scheduler in early generations.
    // No effect on training when task_template_hard_first_generations
    // = 0. Default 1.0 (treat as ordinary task).
    float difficulty{1.0f};
    bool has_protagonist_count{false};
    std::size_t protagonist_count{0};
    bool has_worksite_count{false};
    std::size_t worksite_count{0};
    bool has_tree_count{false};
    std::size_t tree_count{0};
    std::vector<TaskTemplateOverride> background_overrides;  // archetype -> count
    bool has_drive_weight{false};
    double drive_weight{0.0};
    bool has_outcome_weight{false};
    double outcome_weight{1.0};
    bool has_worksite_completion_reward{false};
    float worksite_completion_reward{0.0f};
    bool has_shelter_camp_reward{false};
    float shelter_camp_reward{0.0f};
    bool has_worksite_preactivate{false};
    bool worksite_preactivate{false};
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
    ComputeConfig compute{};
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
    MemoryTrialConfig memory_trial{};
    ProtagonistBrainConfig protagonist{};
    BackgroundBrainConfig background{};
    // L2.4 v2: list of task templates for autocurriculum. When empty
    // (default), the training loop uses the toml's base setup
    // unchanged across all episodes. When non-empty, episodes draw
    // from this list.
    std::vector<TaskTemplate> task_templates{};
    // L2.6 v11: when true (default), all episodes WITHIN one generation
    // see the SAME sampled task_template. The neural net thus has time
    // to learn a stable strategy across the 6-episode evaluation. NEAT
    // selection across generations still rotates through templates.
    // Setting this false restores v10 per-episode resampling, which
    // we found shatters emergent build/hunt/co-presence behaviour
    // because the same network is averaged across 6 conflicting envs.
    bool task_template_stable_within_generation{true};
    // L2.6 v12 (PEARL/RL² context-conditioned policy): when true
    // (default), the protagonist brain receives an 8-onehot input
    // encoding the current task_template index, so a single network
    // can learn task-specific behaviour (proactive_build chops vs
    // wave_attack defends vs mixed_ecology survives) without needing
    // to delete templates from the pool. Set false for ablation.
    bool task_context_perception_enabled{true};
    // L2.7 v13 (HRL worker conditioning): when true (default), the
    // protagonist brain receives a 13-onehot of the current GoalType
    // (Explore/FindWater/.../RestNearFire). Lets the network learn
    // goal-conditioned worker policies on top of the rule-based
    // GoalManager. Set false to ablate (network must reverse-engineer
    // goal from the 1-scalar dim 32 of MemoryCognitionPerception).
    bool goal_context_perception_enabled{true};
    // L2.7 v13 (HRL sub-rewards): per-goal-type completion reward
    // weights. Index = GoalType int value (0..12). reward delta per
    // tick = goal_completion_rewards[g] * agent.goalCompletionThisTick(g).
    // Default values are set in protagonistEcologyConfigFromConfig and
    // tuned to balance roughly with v12 outcome_weight scale (so e.g.
    // a worksite completion gives ~+20 here in addition to the v9-style
    // worksite_completion_reward * outcome_weight). Toml keys live
    // under [reward.goal_completion] (drink, food, gather_stick, etc.).
    std::array<float, 13> goal_completion_rewards{};
    // L2.6 v12 (ACCEL regret-based curriculum): when true, the static
    // [[evolution.task_templates.weight]] is dynamically multiplied by
    // (1 + regret_score) where regret_score is a rolling estimate of
    // (best_fitness - avg_fitness) gap on that template. High-regret
    // templates ("agent ALMOST solves it") are prioritised, which is
    // the standard ACCEL principle (Parker-Holder 2022).
    bool task_template_dynamic_weight{true};
    // L2.6 v12 (Hard-Tasks-First, SMT ICML 2024): in early generations
    // (gen < this many), templates with declared higher difficulty
    // get an extra weight multiplier so the network is forced to
    // confront hard tasks first. After the warm-up window, templates
    // weight equally. Disable by setting to 0.
    std::size_t task_template_hard_first_generations{20};

    // L2.8 v14 ME-NEAT (MAP-Elites + NEAT). Replaces single-fitness NEAT
    // selection with quality-diversity. When enabled, every generation a
    // QualityDiversityArchive is built keyed on a 2D behaviour descriptor
    // (chops x worksites+crafts). Each cell keeps the highest-fitness
    // genome. For breed selection, evaluated[].fitness is shaped so that
    // archive cell elites keep their fitness while non-elites are
    // suppressed by me_neat_non_elite_weight, biasing EliteSelection +
    // TournamentSelection toward cell elites. Stepping stones in long
    // compositional chains (e.g. "chop a lot + build 1 worksite") are
    // preserved because they hold a unique cell rather than being
    // out-fitnessed by a different cell's specialist.
    bool me_neat_enabled{false};
    int me_neat_chops_bin_size{20};       // x-axis bucket: chops per cell
    int me_neat_workcraft_bin_size{2};    // y-axis bucket: (build+craft) per cell
    int me_neat_max_bins{5};              // grid is N x N (default 5x5 = 25 cells)
    // L2.9 v17: optional 3rd BD axis (hunt count). When hunt_max_bins=1
    // (default) the archive is the legacy 2D N x N grid; when > 1 the
    // archive becomes N x N x hunt_max_bins. Used to prevent ME-NEAT
    // archive from squeezing out hunting genes (v15/v16 hunts regression
    // 0.66x-0.66x baseline). Default disabled for backward-compat.
    int me_neat_hunt_bin_size{0};         // z-axis bucket: hunts per cell (0 = disable)
    int me_neat_hunt_max_bins{1};         // z-axis cell count (1 = 2D)
    // When 0 < w < 1, non-elite genomes have their fitness multiplied
    // by w during reproduce. EliteSelection then naturally lifts cell
    // elites into the survivor set, and tournament samples favour them.
    // 0.05 = strong suppression (close to "only cell elites survive");
    // 1.0 = no suppression (archive only used for metrics).
    double me_neat_non_elite_weight{0.05};

    // L2.9 v15 "real" ME-NEAT (Mouret 2015 Tutorial + Nilsson 2021 PGA-MAP-Elites
    // + Vassiliades 2018 iso+line operator). v14/v14b's implicit fitness
    // shaping kept me_coverage stuck at 4% because EliteSelection +
    // Tournament still selected on the v13 fitness gradient; the archive
    // never drove reproduction. v15 routes breeding through the archive
    // itself when archive_breeding=true:
    //
    //   * me_neat_archive_breeding=true: parents are sampled uniformly
    //     from filled archive cells (not tournament). Each cell elite
    //     is preserved as-is (no fitness suppression dance needed).
    //   * me_neat_init_generations: in the first N gens, *all* new
    //     offspring come from random crossovers regardless of fitness,
    //     to seed the archive (Nilsson 2021 "initialization phase").
    //   * me_neat_line_sigma > 0: switch crossover from uniform pick
    //     (a[i] or b[i]) to iso+line: child[i] = a[i] + alpha*(b[i]-a[i])
    //     with alpha ~ N(0, line_sigma). Empirically extends along the
    //     line between two elites, the "elite hypervolume" of Vassiliades
    //     2018, giving 4x more coverage than uniform crossover.
    //   * me_neat_mutation_boost: multiplier on weight_perturb_sigma
    //     used only on v15 archive-breeding path. Default 1.0 = no
    //     change; 2.0+ adds extra exploration noise to break premature
    //     convergence.
    bool me_neat_archive_breeding{false};
    std::size_t me_neat_init_generations{10};
    double me_neat_line_sigma{0.0};
    double me_neat_mutation_boost{1.0};

    bool isTrainRole() const;
    bool isEvalRole() const;
    bool isLiveRole() const;

    static ProtagonistEcologyConfig fromConfig(const config::Config& config);
};

}  // namespace neuro::routes::protagonist
