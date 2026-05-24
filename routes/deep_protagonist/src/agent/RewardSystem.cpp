#include "agent/RewardSystem.hpp"

#include <cmath>

namespace dp::agent {

void RewardSystem::EpisodeStats::reset() {
    steps = 0;
    total_reward = 0.0f;
    r_alive = r_food = r_water = r_kills = r_bites = 0.0f;
    r_death = r_vital = r_collect = r_shelter = r_craft = 0.0f;
    r_milestone = 0.0f;
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
}

void RewardSystem::begin_step() { step_ = StepReward{}; }

void RewardSystem::event_alive(float dt) {
    step_.alive += w_.alive_step * dt;  // alive_step is per-second
}

void RewardSystem::event_eat(float kcal) {
    step_.food += w_.per_food_kcal * kcal;
}

void RewardSystem::event_drink(float ml) {
    step_.water += w_.per_water_ml * ml;
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
                   + std::abs(v.temperature - 50.0f) * 2.0f;
    step_.vital += w_.vital_decay_coef * pressure * dt * -1.0f;  // negative
}

void RewardSystem::event_collected_resource() { step_.collect   += w_.collected; }
void RewardSystem::event_built_shelter()      { step_.shelter   += w_.built_shelter; }
void RewardSystem::event_crafted_tool()       { step_.craft     += w_.crafted_tool; }
void RewardSystem::event_milestone()          { step_.milestone += w_.milestone; }

float RewardSystem::end_step() {
    last_ = step_;
    ep_.add(last_);
    return last_.total();
}

}  // namespace dp::agent
