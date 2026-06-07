#pragma once

#include "agent/CapsuleAgent.hpp"
#include "agent/Inventory.hpp"
#include "agent/RewardSystem.hpp"
#include "agent/Shelter.hpp"
#include "agent/VitalSystem.hpp"
#include "world/AnimalSystem.hpp"
#include "world/BuildingSystem.hpp"
#include "world/PlantSystem.hpp"
#include "world/World.hpp"

#include <cstdint>

namespace dp::agent {

// Owns "what is one episode" - the unit PPO trains over.
//
// Conventions:
//   * One episode = from spawn until either death or wall-clock cap.
//   * Terrain / biome / rivers / caves / decorations are NEVER reset
//     (they're effectively static features of the world).
//   * On reset we re-place the agent at the spawn point, fully restore
//     vitals, clear the per-episode reward accumulator, and reseed the
//     animal layer so each episode starts in a comparable state.
//   * Plants regrow on their own timers, so we leave them alone (matches
//     "berries don't all reappear at once" intuition).
class EpisodeManager {
public:
    struct Config {
        float    max_seconds      = 300.0f;  // 5 minute hard cap per episode
        float    spawn_x          = 0.0f;
        float    spawn_y          = 0.0f;
        float    spawn_clearance  = 5.0f;    // metres above ground at spawn
        uint64_t animal_seed_base = 0xA11AULL;
    };

    void configure(const Config& cfg) { cfg_ = cfg; }
    // D-133: update only the spawn point (keeps animal seed / timing config).
    // Lets Environment randomise the spawn per episode without re-seeding.
    void set_spawn(float x, float y) { cfg_.spawn_x = x; cfg_.spawn_y = y; }

    // Mark a fresh episode: returns id of the new episode.
    int  begin_episode();

    // Advance the wall-clock for the current episode.
    void tick(float dt);

    // True if the current episode should end (death OR time cap).
    bool is_done(const VitalState& v) const;

    // Reset agent to spawn, restore vitals, reset reward + animals,
    // clear inventory / shelter / buildings. Plants and resources are
    // left to regrow naturally; ProgressTracker is intentionally NOT
    // cleared here (curriculum progress persists across episodes by
    // design, matching the "single network from S4 to S6" rule).
    void reset(const dp::world::World&    world,
               CapsuleAgent&               agent,
               VitalSystem&                vital_sys,
               VitalState&                 vital_state,
               RewardSystem&               reward,
               dp::world::AnimalSystem&    animals,
               Inventory&                  inventory,
               Shelter&                    shelter,
               dp::world::BuildingSystem&  buildings);

    // Useful for HUDs / PPO logs.
    int   episode_id()        const { return episode_id_; }
    int   step_count()        const { return step_count_; }
    float elapsed_s()         const { return elapsed_s_; }
    int   episodes_completed() const { return episodes_completed_; }

private:
    Config  cfg_;
    int     episode_id_         = -1;
    int     step_count_         = 0;
    float   elapsed_s_          = 0.0f;
    int     episodes_completed_ = 0;
};

}  // namespace dp::agent
