#pragma once

#include "agent/VitalSystem.hpp"

namespace dp::agent {

// PPO needs a single scalar per tick. We compose it from a small set of
// named events (alive, ate, drank, killed wolf, bitten, died, vital
// pressure, collected, built shelter, crafted) so that:
//
//   1. Adding a new behaviour later means adding a new event + weight,
//      *never* removing an old one. (Final blueprint rule:
//      "every new task keeps old reward weights intact")
//   2. Each component's contribution is logged separately, so we can spot
//      regressions per behaviour.
//
// Usage per tick:
//
//   reward.begin_step();
//   reward.event_alive(dt);
//   if (ate) reward.event_eat(kcal);
//   ...
//   float r = reward.end_step();
//
// The system keeps a per-step breakdown (last_step()) and per-episode
// totals (episode()).
class RewardSystem {
public:
    // Tunable weights. Defaults are sane S4 starters; documented in
    // docs/参数速查.md.
    struct Weights {
        // "Stay alive" payout, applied per simulated second (multiplied
        // by dt internally). 0.05 ⇒ +3 reward per minute alive.
        float alive_step       = 0.05f;

        // Linear payouts for needs satisfaction, scaled to ~+1.0 per
        // typical event (eating one fruit ≈ 25 kcal × 0.04 = +1).
        float per_food_kcal    = 0.04f;
        float per_water_ml     = 0.03f;

        // Wolves
        float wolf_killed      = 5.0f;
        float wolf_bite_dmg    = -1.0f;   // per damage point absorbed

        // Hard terminal
        float death            = -100.0f;

        // Soft pressure when vitals decay (combined "missing %" over the
        // 4 vitals × dt × this coef). Tuned so that fully-fed agent gets
        // ≈0 pressure, fully-starved gets ≈ -0.02/s -> noticeable but not
        // dominating the alive bonus.
        float vital_decay_coef = 0.00005f;

        // Resource / construction (S3.7e/f/g)
        float collected        = 0.5f;
        float built_shelter    = 5.0f;
        float crafted_tool     = 3.0f;
    };

    // Per-step breakdown so we can debug *why* a reward is what it is.
    struct StepReward {
        float alive    = 0.0f;
        float food     = 0.0f;
        float water    = 0.0f;
        float kills    = 0.0f;
        float bites    = 0.0f;
        float death    = 0.0f;
        float vital    = 0.0f;
        float collect  = 0.0f;
        float shelter  = 0.0f;
        float craft    = 0.0f;

        float total() const {
            return alive + food + water + kills + bites + death
                 + vital + collect + shelter + craft;
        }
    };

    // Per-episode running totals. Reset between episodes; logged at
    // episode end.
    struct EpisodeStats {
        int   steps        = 0;
        float total_reward = 0.0f;
        float r_alive   = 0.0f;
        float r_food    = 0.0f;
        float r_water   = 0.0f;
        float r_kills   = 0.0f;
        float r_bites   = 0.0f;
        float r_death   = 0.0f;
        float r_vital   = 0.0f;
        float r_collect = 0.0f;
        float r_shelter = 0.0f;
        float r_craft   = 0.0f;

        void reset();
        void add(const StepReward& s);
    };

    explicit RewardSystem(const Weights& w = {}) : w_(w) {}

    void   begin_step();

    // Tick-level events. Multiple of the same kind in one tick stack.
    void   event_alive(float dt);
    void   event_eat(float kcal);
    void   event_drink(float ml);
    void   event_wolf_killed();
    void   event_wolf_bite(float damage_pts);
    void   event_death();
    // VitalSystem-aware soft pressure: pass the current state and dt; we
    // apply (sum over vitals of (max - current)) * coef * dt.
    void   event_vital_pressure(const VitalState& v, float dt);
    void   event_collected_resource();
    void   event_built_shelter();
    void   event_crafted_tool();

    // Close the step. Pushes per-component into ep_, returns total.
    float  end_step();

    const StepReward&   last_step()  const { return last_; }
    const EpisodeStats& episode()    const { return ep_; }
    void                reset_episode() { ep_.reset(); }
    const Weights&      weights()    const { return w_; }

private:
    Weights      w_;
    StepReward   step_{};
    StepReward   last_{};
    EpisodeStats ep_{};
};

}  // namespace dp::agent
