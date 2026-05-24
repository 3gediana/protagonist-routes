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
    float thirst_decay_per_min = 12.0f;
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

    // Consumption events
    void eat(VitalState& s, float nutrition) const;       // food refills hunger
    void drink(VitalState& s, float amount) const;        // water refills thirst
    void rest(VitalState& s, float dt) const;             // sit still, regen stamina

    // Damage / kill / respawn helpers
    void respawn(VitalState& s) const;

    const VitalConfig& config() const { return cfg_; }

private:
    VitalConfig cfg_;
};

}  // namespace dp::agent
