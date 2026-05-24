#pragma once

#include "world/Daylight.hpp"
#include "world/World.hpp"

#include <cstdint>

namespace dp::agent {

// The four "vital signs" the agent must keep above zero to stay alive.
// All in [0, 100]; 100 = full, 0 = dies (or takes damage and respawns).
struct VitalState {
    float hunger      = 100.0f;
    float thirst      = 100.0f;
    float stamina     = 100.0f;
    float temperature = 50.0f;   // 50 = comfortable; <30 cold; >70 overheating
    bool  alive       = true;
    int   deaths      = 0;
};

struct VitalConfig {
    // Per-minute base decay
    float hunger_decay_per_min = 8.0f;
    // D-040 → D-041 evolution:
    //   12.0/min: drink dominant (r_water +1.8/ep) → agent locks into drinking
    //   4.0/min:  drink dies (r_water 0) → agent loses spatial nav cue → NOOP 52% / death 40%
    //   8.0/min:  middle ground (r_water +1.2/ep, ~half of food magnitude).
    // Combined with D-041 satiation scaling in Environment.cpp (drink reward
    // tapers as thirst rises) and per-episode first_X milestones, this lets
    // PPO chase 8 main-line rewards instead of locking on a single one.
    float thirst_decay_per_min = 8.0f;
    // Stamina dynamics (per second)
    float stamina_run_decay   = 8.0f;   // when speed > run_threshold
    float stamina_walk_decay  = 0.5f;   // when moving normally
    float stamina_recover     = 4.0f;   // when nearly still
    float run_speed_threshold = 4.0f;   // m/s
    // Temperature: cold at night, hot in desert daytime, neutral elsewhere
    float temp_cold_decay_per_min = 6.0f;   // night drift toward 0
    float temp_hot_decay_per_min  = 4.0f;   // desert daytime drift toward 100
    float temp_neutral_recover    = 3.0f;   // recover toward 50 in mild conditions
};

class VitalSystem {
public:
    explicit VitalSystem(const VitalConfig& cfg = {}) : cfg_(cfg) {}

    // Step one frame of vital evolution. `agent_speed` is m/s, used to drive
    // stamina spend. The world + daylight let us figure out biome (desert
    // overheats, swamp rains cooler) and time of day (cold night).
    // `cold_resist` (default 0.0) scales cold drain: 0 = full drain,
    // 1 = no drain at all. Hooks Wardrobe's clothing effect into the
    // existing temperature model.
    void tick(VitalState& s,
              const dp::world::World& world,
              float wx, float wy,
              float agent_speed,
              const dp::world::Daylight& day,
              float dt,
              float cold_resist = 0.0f) const;

    // Consumption events. Both return the *actual* amount of vital
    // restored (0 when the meter was already full); callers pass that
    // delta to RewardSystem so reward shaping reflects what the agent
    // actually got, not the attempted amount. This is the fix for the
    // S4-pre training run where PPO discovered it could spam Q in a
    // river to farm reward despite thirst already being capped at 100.
    float eat(VitalState& s, float nutrition) const;       // food refills hunger
    float drink(VitalState& s, float amount) const;        // water refills thirst
    void  rest(VitalState& s, float dt) const;             // sit still, regen stamina

    // Damage / kill / respawn helpers
    void respawn(VitalState& s) const;

    const VitalConfig& config() const { return cfg_; }

private:
    VitalConfig cfg_;
};

}  // namespace dp::agent
