#pragma once

#include "agent/AgentAction.hpp"
#include "agent/CapsuleAgent.hpp"
#include "agent/EpisodeManager.hpp"
#include "agent/Inventory.hpp"
#include "agent/Observation.hpp"
#include "agent/ProgressTracker.hpp"
#include "agent/RewardSystem.hpp"
#include "agent/Shelter.hpp"
#include "agent/TameSystem.hpp"
#include "agent/VitalSystem.hpp"
#include "agent/Wardrobe.hpp"
#include "world/AnimalSystem.hpp"
#include "world/BuildingSystem.hpp"
#include "world/Daylight.hpp"
#include "world/Decorations.hpp"
#include "world/FarmingSystem.hpp"
#include "world/Physics.hpp"
#include "world/PlantSystem.hpp"
#include "world/ResourceSystem.hpp"
#include "world/World.hpp"

#include <cstdint>

namespace dp::env {

// Configuration of one survival environment instance.
struct EnvConfig {
    // Same fields as the built-in World::Config, but propagated through here
    // so headless trainers don't have to load TOML themselves.
    dp::world::World::Config world;

    // Episode boundaries
    float    max_episode_seconds = 300.0f;

    // Headless tick rate. Real-time human play uses raylib's variable dt;
    // headless training uses a fixed dt so step counts ≡ wall-clock seconds.
    float    fixed_dt            = 1.0f / 30.0f;   // 30 Hz default

    // Per-instance seed offset (lets multiple envs share a world but not
    // animal layout / RNG).
    uint64_t instance_seed_xor   = 0xA9E1ULL;
};

// One step worth of "what happened" for a learner to consume.
struct StepResult {
    dp::agent::ObservationBuilder::Vector obs{};   // 145 floats
    float                                  reward = 0.0f;
    bool                                   done   = false;
    int                                    episode_id   = 0;
    int                                    episode_step = 0;
};

// Headless wrapper around the full survival simulation. Mirrors what
// main.cpp's main loop does, minus rendering and minus keyboard handling
// (the AgentAction passed into step() is the only input channel).
//
// Intended consumers:
//   * Main viewer (raylib build): currently still inlines everything in
//     main.cpp so we don't risk regressions. Will fold in a later pass.
//   * deep_protagonist_train binary: drives the env headless, eventually
//     fed by a PPO policy.
//
// Notes for the trainer:
//   * reset() returns the initial obs *and* counts as starting a new
//     episode. Call it once before the first step, and again after every
//     done.
//   * step() returns done=true on the same tick the episode ends; the
//     caller should call reset() before the next step.
//   * Ticks at fixed_dt regardless of wall clock - 100k steps at 30 Hz
//     ≈ 55 minutes of in-sim time, plenty for short rollouts.
class Environment {
public:
    explicit Environment(const EnvConfig& cfg);

    // Reset full episode state (agent + vitals + animals + inventory +
    // shelter + buildings + farming + tame + episode counters). Terrain,
    // plants, resources are persistent (matches blueprint rule "world
    // never resets, episode does"). Returns the initial obs in obs_out.
    void reset(dp::agent::ObservationBuilder::Vector& obs_out);

    // Advance one tick using the given action. Returns the resulting obs,
    // the per-tick reward, and a done flag. After done==true the caller
    // MUST call reset() before stepping again.
    StepResult step(const dp::agent::AgentAction& action);

    // -- Read-only views for loggers / debug HUDs ---------------------------
    int                                       observation_size() const {
        return dp::agent::ObservationBuilder::SIZE;
    }
    const dp::agent::RewardSystem::EpisodeStats& last_episode_stats() const {
        return last_ep_stats_;
    }
    int                                       episodes_completed() const {
        return episodes_completed_;
    }
    const dp::agent::VitalState&              vitals() const { return vital_state_; }
    const dp::agent::CapsuleAgent&            agent() const { return agent_; }
    const dp::agent::ProgressTracker&         progress() const { return progress_; }
    const dp::agent::Inventory&               inventory() const { return inventory_; }
    const dp::world::BuildingSystem&          buildings() const { return buildings_; }
    const dp::world::FarmingSystem&           farming() const { return farming_; }
    const dp::agent::TameSystem&              tame() const { return tame_; }
    const dp::agent::Wardrobe&                wardrobe() const { return wardrobe_; }
    int                                       step_index() const { return step_index_; }

private:
    // Compose the current observation into out_.
    void rebuild_observation_(dp::agent::ObservationBuilder::Vector& out) const;

    // Apply one AgentAction's effects (mirrors main.cpp's keyboard branch).
    void apply_action_effects_(const dp::agent::AgentAction& action);

    EnvConfig                       cfg_;
    float                           spawn_x_ = 0.0f;
    float                           spawn_y_ = 0.0f;
    float                           spawn_z_ = 0.0f;

    // World-static (never reset across episodes)
    dp::world::World                world_;
    dp::world::Decorations          decorations_;
    dp::world::Daylight             day_;
    dp::world::PlantSystem          plants_;
    dp::world::ResourceSystem       resources_;
    dp::world::AnimalSystem         animals_;
    dp::world::Physics              physics_;

    // Per-episode systems
    dp::agent::CapsuleAgent         agent_;
    dp::agent::VitalSystem          vitals_;
    dp::agent::VitalState           vital_state_{};
    dp::agent::Inventory            inventory_;
    dp::agent::Shelter              shelter_;
    dp::agent::Wardrobe             wardrobe_;
    dp::world::BuildingSystem       buildings_;
    dp::world::FarmingSystem        farming_;
    dp::agent::TameSystem           tame_;
    dp::agent::ProgressTracker      progress_;
    dp::agent::RewardSystem         reward_;
    dp::agent::EpisodeManager       episodes_;

    // S3.10 main-loop scratch state (matches main.cpp local vars).
    dp::world::BuildingType         selected_building_ = dp::world::BuildingType::Shed;
    dp::world::PlantKind            last_seed_kind_    = dp::world::PlantKind::Berry;
    bool                            first_house_built_fired_    = false;
    bool                            first_crop_harvested_fired_ = false;
    bool                            first_follower_fired_       = false;
    bool                            was_alive_                  = true;

    // Logging / metrics scratch.
    int                                          episodes_completed_ = 0;
    int                                          step_index_         = 0;
    dp::agent::RewardSystem::EpisodeStats        last_ep_stats_{};
};

}  // namespace dp::env
