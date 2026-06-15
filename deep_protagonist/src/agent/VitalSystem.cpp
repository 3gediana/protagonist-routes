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
                       float cold_resist,
                       float warmth,
                       float slope01,
                       float wet_cold_mult) const {
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

    // Stamina depends on movement speed; climbing steep terrain (slope01,
    // S5 world realism) multiplies the drain while moving so that scaling a
    // mountain genuinely tires the agent. slope01=0 (flat) leaves the old
    // behaviour exactly intact.
    float sc = slope01 < 0.0f ? 0.0f : (slope01 > 1.0f ? 1.0f : slope01);
    float slope_cost = 1.0f + cfg_.stamina_slope_mult * sc;
    if (agent_speed > cfg_.run_speed_threshold) {
        s.stamina -= cfg_.stamina_run_decay * dt * slope_cost;
    } else if (agent_speed > 0.5f) {
        s.stamina -= cfg_.stamina_walk_decay * dt * slope_cost;
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
        // Stage W1 world coherence: a rain-soaked body sheds heat faster.
        // wet_cold_mult is 1.0 (dry / weather off) up to ~2.0 (drenched),
        // computed by Environment from VitalState.wetness. Multiplicative on
        // the cold drain so getting caught in the rain at night is a real
        // threat the agent must answer (re-light fire / shelter / clothes).
        if (wet_cold_mult > 1.0f) cold_rate *= wet_cold_mult;
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

    // D-112: active heat source (a nearby lit campfire) pulls body
    // temperature up toward a warm target, regardless of biome / night, and
    // is applied in THIS tick before the death check so a fuelled fire keeps
    // the agent alive instead of correcting one tick too late. Causal: the
    // Environment only passes warmth>0 while the agent is within a lit
    // fire's radius (fuel burns down -> warmth disappears). Capped below 100
    // so a fire can never cause an overheating death.
    if (warmth > 0.0f) {
        constexpr float kWarmTarget = 60.0f;  // cozy, well clear of 100 burn
        s.temperature = move_toward(s.temperature, kWarmTarget, warmth * dt_min);
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
        if (freeze_or_burn) s.deaths_cold += 1;   // D-112 diag (read-only)
        else                s.deaths_food += 1;
    }

    // decision_blueprint_nutrition (round_21): dual nutrition bars. Entirely
    // gated by nutrition_enabled; when off this block never runs and the bars
    // stay at their 100/100 default, so old-world checkpoints are unaffected.
    // Decay -> clamp -> grace-timed death (separate cause bookkeeping). No
    // reward is touched here; malnutrition only weakens (nutrition_speed_mult)
    // and eventually kills, never pays out (anti-Goodhart per blueprint).
    if (cfg_.nutrition_enabled && s.alive) {
        s.protein -= cfg_.protein_decay_per_min * dt_min;
        s.vitamin -= cfg_.vitamin_decay_per_min * dt_min;
        s.protein  = clamp01_100(s.protein);
        s.vitamin  = clamp01_100(s.vitamin);
        s.protein_zero_s = (s.protein <= 0.0f) ? s.protein_zero_s + dt : 0.0f;
        s.vitamin_zero_s = (s.vitamin <= 0.0f) ? s.vitamin_zero_s + dt : 0.0f;
        bool die_protein = s.protein_zero_s > cfg_.nutri_death_grace_s;
        bool die_vitamin = s.vitamin_zero_s > cfg_.nutri_death_grace_s;
        if (die_protein || die_vitamin) {
            s.alive = false;
            s.deaths += 1;
            if (die_protein) s.deaths_protein += 1;
            else             s.deaths_vitamin += 1;
        }
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

float VitalSystem::eat_protein(VitalState& s, float amount) const {
    if (!cfg_.nutrition_enabled || amount <= 0.0f) return 0.0f;
    float old = s.protein;
    s.protein = clamp01_100(s.protein + amount);
    if (s.protein > 0.0f) s.protein_zero_s = 0.0f;
    return s.protein - old;
}

float VitalSystem::eat_vitamin(VitalState& s, float amount) const {
    if (!cfg_.nutrition_enabled || amount <= 0.0f) return 0.0f;
    float old = s.vitamin;
    s.vitamin = clamp01_100(s.vitamin + amount);
    if (s.vitamin > 0.0f) s.vitamin_zero_s = 0.0f;
    return s.vitamin - old;
}

float VitalSystem::nutrition_speed_mult(const VitalState& s) const {
    if (!cfg_.nutrition_enabled) return 1.0f;
    const float thresh = cfg_.nutri_debuff_thresh * 100.0f;
    auto barmul = [&](float v) -> float {
        if (v >= thresh) return 1.0f;
        float frac = (thresh > 0.0f) ? (v / thresh) : 0.0f; // 1 at thresh -> 0 at empty
        if (frac < 0.0f) frac = 0.0f;
        if (frac > 1.0f) frac = 1.0f;
        return cfg_.nutri_debuff_floor + (1.0f - cfg_.nutri_debuff_floor) * frac;
    };
    return barmul(s.protein) * barmul(s.vitamin);
}

void VitalSystem::respawn(VitalState& s) const {
    s.hunger = 100.0f;
    s.thirst = 100.0f;
    s.stamina = 100.0f;
    s.temperature = 50.0f;
    s.alive = true;
    s.protein = 100.0f;
    s.vitamin = 100.0f;
    s.protein_zero_s = 0.0f;
    s.vitamin_zero_s = 0.0f;
}

}  // namespace dp::agent
