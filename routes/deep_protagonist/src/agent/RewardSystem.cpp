#include "agent/RewardSystem.hpp"

#include <algorithm>
#include <cmath>

namespace dp::agent {

void RewardSystem::EpisodeStats::reset() {
    steps = 0;
    total_reward = 0.0f;
    r_alive = r_food = r_water = r_kills = r_bites = 0.0f;
    r_death = r_vital = r_collect = r_shelter = r_craft = 0.0f;
    r_milestone = 0.0f;
    r_potential = 0.0f;
}

void RewardSystem::EpisodeStats::add(const StepReward& s) {
    steps        += 1;
    total_reward += s.total();
    r_alive     += s.alive;
    r_food      += s.food;
    r_water     += s.water;
    r_kills     += s.kills;
    r_bites     += s.bites;
    r_death     += s.death;
    r_vital     += s.vital;
    r_collect   += s.collect;
    r_shelter   += s.shelter;
    r_craft     += s.craft;
    r_milestone += s.milestone;
    r_potential += s.potential;
}

void RewardSystem::begin_step() { step_ = StepReward{}; }

void RewardSystem::event_alive(float dt) {
    step_.alive += w_.alive_step * dt;  // alive_step is per-second
}

// D-043: kcal/ml semantics replaced by drive_reduction (Keramati 2014).
// Callers pass drive_before-drive_after ∈ [0, 1]; coef ~3.0 keeps single
// eat/drink event reward in Crafter-aligned magnitude.
void RewardSystem::event_eat(float drive_red) {
    step_.food += w_.food_coef * drive_red;
}

void RewardSystem::event_drink(float drive_red) {
    step_.water += w_.water_coef * drive_red;
}

void RewardSystem::event_wolf_killed() {
    step_.kills += w_.wolf_killed;
}

void RewardSystem::event_wolf_bite(float damage_pts) {
    step_.bites += w_.wolf_bite_dmg * damage_pts;  // wolf_bite_dmg is negative
}

void RewardSystem::event_death() {
    step_.death += w_.death;
}

void RewardSystem::event_action_cost(float cost) {
    // Pay a small fee for *trying* a discrete trigger. Folded into the
    // vital bucket so we don't have to grow the 11-component schema.
    step_.vital -= cost;
}

void RewardSystem::event_vital_pressure(const VitalState& v, float dt) {
    // Sum of "how far off comfortable" across the four vitals. Each is
    // in [0, 100]. For temperature we anchor on 50 (the "neutral" point
    // documented in VitalSystem.hpp), so swinging in either direction is
    // a penalty.
    float pressure = (100.0f - v.hunger)
                   + (100.0f - v.thirst)
                   + (100.0f - v.stamina)
                   + std::abs(v.temperature - 50.0f) * 2.0f
                   // decision_blueprint_nutrition (round_21): the two nutrition
                   // bars carry the SAME survival-pressure shape as hunger/thirst
                   // (a penalty for being depleted, never a reward for eating).
                   // When nutrition is disabled the bars stay at 100, so both
                   // terms are exactly 0 and the old reward landscape is intact.
                   + (100.0f - v.protein)
                   + (100.0f - v.vitamin);
    step_.vital += w_.vital_decay_coef * pressure * dt * -1.0f;  // negative
}

void RewardSystem::event_collected_resource() { step_.collect   += w_.collected; }
void RewardSystem::event_deposited_material() { step_.collect   += w_.deposited; }
void RewardSystem::event_built_shelter()      { step_.shelter   += w_.built_shelter; }
void RewardSystem::event_crafted_tool()       { step_.craft     += w_.crafted_tool; }
void RewardSystem::event_milestone(float scale) {
    step_.milestone += w_.milestone * std::clamp(scale, 0.0f, 1.0f);
}

void RewardSystem::event_state_bonus(float phi) {
    // D-043 P2: per-step state-based reward. phi is expected to be a small
    // non-negative quantity (~ [0, 1.5]) representing accumulated state
    // (house, clothes, followers, inventory diversity). Clamp defensively.
    step_.potential += w_.state_bonus_coef * std::max(0.0f, phi);
}

float RewardSystem::end_step() {
    // D-043 P1: clamp the per-step total to keep PPO advantage estimation
    // stable. Individual components are not clamped (so EpisodeStats reflects
    // the "intended" reward), only the scalar returned to the learner.
    last_ = step_;
    ep_.add(last_);
    float total = last_.total();
    return std::clamp(total, w_.reward_clip_min, w_.reward_clip_max);
}

}  // namespace dp::agent
