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

void RewardSystem::begin_step() {
    step_ = StepReward{};
    // D-123 SF: reset the parallel un-weighted φ accumulator each step so
    // last_features() reflects only this step (mirrors begin_step on step_).
    feat_.fill(0.0f);
}

void RewardSystem::event_alive(float dt) {
    step_.alive += w_.alive_step * dt;  // alive_step is per-second
    feat_[0] += dt;                     // φ alive = seconds alive (w = alive_step)
}

// D-043: kcal/ml semantics replaced by drive_reduction (Keramati 2014).
// Callers pass drive_before-drive_after ∈ [0, 1]; coef ~3.0 keeps single
// eat/drink event reward in Crafter-aligned magnitude.
void RewardSystem::event_eat(float drive_red) {
    step_.food += w_.food_coef * drive_red;
    feat_[1] += drive_red;             // φ food = drive_reduction (w = food_coef)
}

void RewardSystem::event_drink(float drive_red) {
    step_.water += w_.water_coef * drive_red;
    feat_[2] += drive_red;             // φ water = drive_reduction (w = water_coef)
}

void RewardSystem::event_wolf_killed() {
    step_.kills += w_.wolf_killed;
    feat_[3] += 1.0f;                  // φ kills = count (w = wolf_killed)
}

void RewardSystem::event_wolf_bite(float damage_pts) {
    step_.bites += w_.wolf_bite_dmg * damage_pts;  // wolf_bite_dmg is negative
    feat_[4] += damage_pts;            // φ bites = damage points (w = wolf_bite_dmg<0)
}

void RewardSystem::event_death() {
    step_.death += w_.death;
    feat_[5] += 1.0f;                  // φ death = 1 (w = death)
}

void RewardSystem::event_action_cost(float cost) {
    // Pay a small fee for *trying* a discrete trigger. Folded into the
    // vital bucket so we don't have to grow the 11-component schema.
    step_.vital -= cost;
    feat_[6]   -= cost;                // φ vital is a BLEND stored at w=1
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
    const float term = w_.vital_decay_coef * pressure * dt * -1.0f;  // negative
    step_.vital += term;
    feat_[6]    += term;              // φ vital BLEND mirrors step_.vital (w=1)
}

void RewardSystem::event_vital_danger(const VitalState& v, float dt) {
    // D-122 credit-assignment refinement. CONVEX (squared) per-vital deficit so
    // the single vital being neglected dominates the penalty (root-cause), vs
    // the legacy LINEAR pressure which smears blame evenly. Each term is
    // ((100-vital)/100)^2 in [0,1]; a comfortable vital (>=75) contributes
    // <=0.0625, a critical one (<=10) contributes >=0.81. Summed and scaled by
    // vital_danger_coef * dt, applied as a negative. This sharpens the gradient
    // (and the GAE back-propagation of the eventual death) onto the decisions
    // that let THAT vital slide. Folded into the `vital` bucket (schema intact).
    auto sq_def = [](float vital) -> float {
        float gap = (100.0f - std::clamp(vital, 0.0f, 100.0f)) / 100.0f;
        return gap * gap;
    };
    float danger = sq_def(v.hunger)
                 + sq_def(v.thirst)
                 + sq_def(v.stamina)
                 // temperature: distance from the 50 neutral, normalised to the
                 // 50-wide half-range so a full swing to 0 or 100 reads as 1.0.
                 + [&]{ float g = std::abs(v.temperature - 50.0f) / 50.0f;
                        return g * g; }()
                 + sq_def(v.protein)
                 + sq_def(v.vitamin);
    const float term = w_.vital_danger_coef * danger * dt * -1.0f;  // negative
    step_.vital += term;
    feat_[6]    += term;              // φ vital BLEND mirrors step_.vital (w=1)
}

// collect/shelter/craft/milestone: collect (idx 7) is a BLEND of collected +
// deposited, so φ₇ stores the already-weighted increment (w=1) to mirror
// step_.collect; the rest store the raw count with their single weight.
void RewardSystem::event_collected_resource() { step_.collect   += w_.collected;     feat_[7] += w_.collected; }
void RewardSystem::event_deposited_material() { step_.collect   += w_.deposited;     feat_[7] += w_.deposited; }
void RewardSystem::event_built_shelter()      { step_.shelter   += w_.built_shelter; feat_[8] += 1.0f; }
void RewardSystem::event_crafted_tool()       { step_.craft     += w_.crafted_tool;  feat_[9] += 1.0f; }
void RewardSystem::event_milestone(float scale) {
    const float s = std::clamp(scale, 0.0f, 1.0f);
    step_.milestone += w_.milestone * s;
    feat_[10]       += s;             // φ milestone = clamped scale (w = milestone)
}

// D-128b: direct sparse reward (NOT a potential, so feat_ is untouched) for
// the first prey blow of an episode. Caller gates it (env DP_PREYBLOW_REW)
// and fires it at most once per episode on a rabbit/deer hit.
void RewardSystem::event_prey_blow(float v) { step_.milestone += v; }

// D-131: instrumental coverage bonus, paid once per new map cell this life.
// Routed to milestone (like prey_blow) so it never reads as a real kill/score.
void RewardSystem::event_explore(float v) { step_.milestone += v; }

void RewardSystem::event_state_bonus(float phi) {
    // D-043 P2: per-step state-based reward. phi is expected to be a small
    // non-negative quantity (~ [0, 1.5]) representing accumulated state
    // (house, clothes, followers, inventory diversity). Clamp defensively.
    const float p = std::max(0.0f, phi);
    step_.potential += w_.state_bonus_coef * p;
    feat_[11]       += p;             // φ potential = clamped Φ(s) (w = state_bonus_coef)
}

float RewardSystem::end_step() {
    // D-043 P1: clamp the per-step total to keep PPO advantage estimation
    // stable. Individual components are not clamped (so EpisodeStats reflects
    // the "intended" reward), only the scalar returned to the learner.
    last_ = step_;
    last_feat_ = feat_;               // D-123 SF: snapshot φ parallel to last_
    ep_.add(last_);
    float total = last_.total();
    return std::clamp(total, w_.reward_clip_min, w_.reward_clip_max);
}

}  // namespace dp::agent
