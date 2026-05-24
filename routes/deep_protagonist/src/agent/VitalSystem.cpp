#include "agent/VitalSystem.hpp"

#include <algorithm>
#include <cmath>

namespace dp::agent {

namespace {
inline float clamp01_100(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 100.0f) return 100.0f;
    return v;
}
inline float move_toward(float v, float target, float step) {
    if (v < target) return std::min(target, v + step);
    if (v > target) return std::max(target, v - step);
    return v;
}
}  // namespace

void VitalSystem::tick(VitalState& s,
                       const dp::world::World& world,
                       float wx, float wy,
                       float agent_speed,
                       const dp::world::Daylight& day,
                       float dt,
                       float cold_resist) const {
    if (!s.alive) return;

    const float dt_min = dt / 60.0f;

    // Hunger and thirst decay continuously. Desert daylight accelerates
    // thirst by 2x (blueprint line 20: "沙漠快速失水").
    auto biome_now = world.biome_at(wx, wy);
    float sun_now  = day.sun_height_normalized();
    float thirst_mult = 1.0f;
    if (biome_now == dp::world::Biome::Desert && sun_now > 0.4f) {
        thirst_mult = 2.0f;
    }
    s.hunger -= cfg_.hunger_decay_per_min * dt_min;
    s.thirst -= cfg_.thirst_decay_per_min * dt_min * thirst_mult;

    // Stamina depends on movement speed
    if (agent_speed > cfg_.run_speed_threshold) {
        s.stamina -= cfg_.stamina_run_decay * dt;
    } else if (agent_speed > 0.5f) {
        s.stamina -= cfg_.stamina_walk_decay * dt;
    } else {
        s.stamina += cfg_.stamina_recover * dt;
    }

    // Temperature depends on biome and time of day
    auto biome = biome_now;
    float sun_h = sun_now;
    bool is_night = sun_h < -0.1f;
    bool desert_day = (biome == dp::world::Biome::Desert) && (sun_h > 0.4f);
    bool mountain = (biome == dp::world::Biome::Mountain);

    if (is_night || mountain) {
        // Drift toward cold (0) - mountain or night strips heat. Clothing
        // (cold_resist in [0,1]) damps this drain.
        float cold_rate = cfg_.temp_cold_decay_per_min;
        if (mountain && is_night) cold_rate *= 1.5f;
        float resist = cold_resist;
        if (resist < 0.0f) resist = 0.0f;
        if (resist > 0.95f) resist = 0.95f;
        s.temperature -= cold_rate * dt_min * (1.0f - resist);
    } else if (desert_day) {
        // Drift toward hot (100)
        s.temperature += cfg_.temp_hot_decay_per_min * dt_min;
    } else {
        // Mild: slow recover toward 50
        s.temperature = move_toward(s.temperature, 50.0f,
                                    cfg_.temp_neutral_recover * dt_min);
    }

    s.hunger      = clamp01_100(s.hunger);
    s.thirst      = clamp01_100(s.thirst);
    s.stamina     = clamp01_100(s.stamina);
    s.temperature = clamp01_100(s.temperature);

    // Death check: any vital at 0, or temperature extreme
    bool starve_or_thirst = s.hunger <= 0.0f || s.thirst <= 0.0f;
    bool freeze_or_burn   = s.temperature <= 0.0f || s.temperature >= 100.0f;
    if (starve_or_thirst || freeze_or_burn) {
        s.alive = false;
        s.deaths += 1;
    }
}

float VitalSystem::eat(VitalState& s, float nutrition) const {
    float old = s.hunger;
    s.hunger  = clamp01_100(s.hunger + nutrition);
    return s.hunger - old;
}

float VitalSystem::drink(VitalState& s, float amount) const {
    float old = s.thirst;
    s.thirst  = clamp01_100(s.thirst + amount);
    return s.thirst - old;
}

void VitalSystem::rest(VitalState& s, float dt) const {
    s.stamina = clamp01_100(s.stamina + cfg_.stamina_recover * dt);
}

void VitalSystem::respawn(VitalState& s) const {
    s.hunger = 100.0f;
    s.thirst = 100.0f;
    s.stamina = 100.0f;
    s.temperature = 50.0f;
    s.alive = true;
}

}  // namespace dp::agent
