#pragma once

#include "agent/VitalSystem.hpp"

#include <array>

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
        // (D-051 tried 0.02 to lift drink/eat above the alive baseline,
        // but the agent stopped valuing survival entirely - food collapsed
        // and death% spiked to 60 %. Reverted in D-052; the louder
        // first-eat/drink critical bonus is enough to break NOOP without
        // sacrificing alive incentive.)
        float alive_step       = 0.05f;

        // D-043: payouts for needs satisfaction. Semantic change from
        // "linear in amount" to "linear in drive reduction" (Keramati 2014
        // homeostatic RL). Callers now pass drive_before-drive_after instead
        // of kcal/ml. drive(v) = ((100-v)/100)^2, so drive_red ∈ [0, 1] per
        // call; coef 3.0 means a full hunger→full takes ~3.0 reward across
        // D-048 blueprint回归: drive_red×6 (was ×3). Keramati 2014 quadratic
        // drive yields tiny rewards near full vitals; ×3 made drink/eat
        // signal subordinate to the +0.05/s alive bonus and to "do nothing"
        // (sleep) tactics. ×6 makes "drink to fullness when thirsty" worth
        // ~+1.5 in one event - on par with milestone events - so PPO can
        // actually see the gradient toward water/food. The drive shape
        // still naturally bounds reward (full → 0) so this is not a
        // reward-hacking risk like a flat per-ml carrot would be.
        float food_coef        = 6.0f;
        float water_coef       = 6.0f;

        // Wolves
        // D-043: align with Crafter "defeat_zombie/skeleton +1". 5.0 was a
        // big chunk of typical episode return and combined with first_hunt
        // produced the "撩狼" pathology in run7. +1 keeps the self-defense
        // D-047: wolf_killed reward set to 0. D-043 P1 already removed
        // first_hunt milestone (Crafter has no hunt achievement), but
        // wolf_killed +1 was kept and turned out to be the residual carrot
        // driving 撩狼 collapse: attack wolf had positive expected value
        // while clip [-10,+10] capped the bite-cluster cost, making
        // "small risk with reward" the dominant policy in d046_2M (last 20
        // R=-31.7, water=0, death=40%). Wolves are now pure danger.
        float wolf_killed      = 0.0f;
        float wolf_bite_dmg    = -1.0f;   // per damage point absorbed

        // Hard terminal
        // D-043: -100 was 5x bigger than Crafter's death penalty (max ~10
        // health loss). Combined with +20 first_X spikes it made death
        // gambling worthwhile. -10 keeps death dominant but proportional.
        float death            = -10.0f;

        // Soft pressure when vitals decay (combined "missing %" over the
        // 4 vitals × dt × this coef). Tuned so that fully-fed agent gets
        // ≈0 pressure, fully-starved gets ≈ -0.02/s -> noticeable but not
        // dominating the alive bonus.
        float vital_decay_coef = 0.00005f;

        // D-122 credit-assignment refinement ("precise root-cause" ask). The
        // legacy vital_decay penalty above is LINEAR in each deficit, so a
        // single vital cratering (thirst=5) is blamed no more per-unit than
        // four vitals being mildly low (each at 75). That smears the gradient.
        // event_vital_danger adds a CONVEX (squared) per-vital term so the one
        // vital you are neglecting dominates the penalty -> PPO's gradient (and
        // GAE back-propagation of the eventual death) lands precisely on the
        // decisions that let THAT specific vital slide, instead of a flat blame.
        // It is an EARLY WARNING (ramps up smoothly as a vital approaches 0,
        // long before the terminal -10), so the agent is punished on the neglect
        // window, not only at the moment of death. Only CALLED when cook_enabled
        // (see Environment), so the v7 champion reward landscape is unchanged.
        // Coef 0.02: one critical vital (~0.81 sq) -> ~-0.016/s = -1/min while
        // neglected; negligible when all vitals are comfortable.
        float vital_danger_coef = 0.02f;

        // Resource / construction (S3.7e/f/g)
        float collected        = 0.5f;
        // D-059: rollback to original 5. D-056 tried 15, D-057 tried 10,
        // both collapsed PPO from D-055 ckpt - any reward landscape change
        // on top of a 280-ep ckpt knocks the value head out of calibration.
        // Approach changed in D-059: only modify the WORLD (Shed recipe
        // 6 -> 2 deposits) so the same policy can complete buildings.
        float built_shelter    = 5.0f;
        float crafted_tool     = 3.0f;
        // D-055: dedicated reward for depositing a material into an active
        // construction site. Larger than `collected` (0.5) so PPO can
        // actually see "deposit > collect" gradient. With Shed costing 6
        // materials, depositing all the way gives +6 reward across 6 steps,
        // i.e. a denser breadcrumb trail toward the +5 built_shelter prize
        // at the end. D-053 eval showed deposit was 9.4% of triggers but
        // produced 0 buildings - the long horizon was uncrossable. This
        // creates a per-step shaping bonus the way Crafter's per-achievement
        // 1's do, without inventing new milestones.
        float deposited        = 1.0f;

        // S3.10/D-041 main-line milestones (first house, first crop, first
        // follower, stage unlock).
        // D-043: down from 20.0 to 1.0 to align with Crafter's +1/achievement
        // convention. Run7 result confirmed that +20 per first_X dwarfed the
        // -100 death penalty by 1/5, making "gamble for a milestone then die"
        // worthwhile (eg. attacking wolves to get first_hunt). At +1 the agent
        // sees milestones as bonus, not as gambling target.
        float milestone        = 1.0f;

        // D-043 P1: per-step total reward clip. Prevents single-step outliers
        // from polluting PPO advantage estimation. -10 keeps death intact;
        // +10 covers worst case (shelter +5 + craft +3 + milestone +1 +
        // collect +0.5 = ~9.5). Wolf bite is already scaled by dt so per-tick
        // bite ≈ -0.83 — well within range.
        float reward_clip_min  = -10.0f;
        float reward_clip_max  = +10.0f;

        // D-043 P2: state-based bonus coefficient. Each step the agent gets
        // w * Φ(s) added to its reward, where Φ is a hand-crafted potential
        // function over "things the agent has accumulated" (house, clothes,
        // followers, inventory diversity). This is NOT strict PBRS (which
        // would use γΦ' - Φ and yield negative per-step bonus while holding
        // a state) — we want the opposite: holding Φ should give a small
        // ongoing positive payout so the agent learns "I have a home" as a
        // persistent value. Trade-off: violates Ng 1999 policy-invariance,
        // but matches user's "developmental sense" requirement. Coef 0.005
        // means full Φ≈1.5 → +0.0075/step → ~+67 over a 9000-step episode,
        // dominant enough to shape behaviour but well within the clip.
        float state_bonus_coef = 0.005f;
    };

    // Per-step breakdown so we can debug *why* a reward is what it is.
    struct StepReward {
        float alive     = 0.0f;
        float food      = 0.0f;
        float water     = 0.0f;
        float kills     = 0.0f;
        float bites     = 0.0f;
        float death     = 0.0f;
        float vital     = 0.0f;
        float collect   = 0.0f;
        float shelter   = 0.0f;
        float craft     = 0.0f;
        float milestone = 0.0f;
        // D-043 P2: per-step state-bonus from Φ(s). See Weights::state_bonus_coef.
        float potential = 0.0f;

        float total() const {
            return alive + food + water + kills + bites + death
                 + vital + collect + shelter + craft + milestone + potential;
        }
    };

    // Per-episode running totals. Reset between episodes; logged at
    // episode end.
    struct EpisodeStats {
        int   steps        = 0;
        float total_reward = 0.0f;
        float r_alive     = 0.0f;
        float r_food      = 0.0f;
        float r_water     = 0.0f;
        float r_kills     = 0.0f;
        float r_bites     = 0.0f;
        float r_death     = 0.0f;
        float r_vital     = 0.0f;
        float r_collect   = 0.0f;
        float r_shelter   = 0.0f;
        float r_craft     = 0.0f;
        float r_milestone = 0.0f;
        // D-043 P2: episode total of per-step Φ-based bonuses.
        float r_potential = 0.0f;

        void reset();
        void add(const StepReward& s);
    };

    // ------------------------------------------------------------------
    // D-123 Successor-Features (ψ) support. The total reward is a LINEAR
    // weighted sum of named components: reward = Σᵢ wᵢ·φᵢ. Exposing the
    // UN-weighted feature vector φ (last_features) and the weight vector w
    // (weight_vector) lets a learned successor head ψ(s) (discounted future
    // Σφ) estimate "value under a NEW weight recipe w'" as ψ(s)·w' WITHOUT
    // retraining (see docs/Successor_Features_设计.md). F is append-only:
    // adding a future reward term grows F by 1, never reorders/removes.
    //
    // φ channel layout (index : meaning : weight in weight_vector()):
    //   0 alive     : seconds alive this step       : alive_step
    //   1 food      : drive_reduction from eating    : food_coef
    //   2 water     : drive_reduction from drinking   : water_coef
    //   3 kills      : wolves killed this step (count) : wolf_killed
    //   4 bites      : bite damage points absorbed     : wolf_bite_dmg (<0)
    //   5 death      : 1 if died this step else 0       : death
    //   6 vital      : BLENDED bucket (decay+danger+action_cost), stored
    //                  already-summed                  : 1.0  (frozen blend)
    //   7 collect    : BLENDED bucket (collected+deposited), already-summed
    //                                                    : 1.0  (frozen blend)
    //   8 shelter    : shelters built this step (count) : built_shelter
    //   9 craft      : tools crafted this step (count)  : crafted_tool
    //  10 milestone  : milestone scale fired this step  : milestone
    //  11 potential  : clamped Φ(s) state value          : state_bonus_coef
    //
    // The two BLENDED buckets (vital, collect) fold several sub-weights into
    // one StepReward field, so they are stored as the already-weighted sum
    // with w=1 (you can scale the whole bucket via w', but not its internal
    // split). The other 10 store the raw quantity so their single weight is
    // freely re-weightable for recipe screening (this is the point: e.g.
    // "what if kills weight x3"). Invariant (self-tested): dot(last_features,
    // weight_vector) == last_step().total() exactly.
    static constexpr int F = 12;

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
    // D-122: convex per-vital early-warning penalty (see vital_danger_coef).
    // Folded into the `vital` bucket so the jsonl schema is unchanged.
    void   event_vital_danger(const VitalState& v, float dt);
    void   event_collected_resource();
    // D-055: deposit gets its own reward path so PPO sees "deposit > collect"
    // without inflating the collect bucket (which would silently make the
    // wander baseline look stronger too). Folded into step_.collect so the
    // jsonl schema stays at 11 components.
    void   event_deposited_material();
    void   event_built_shelter();
    void   event_crafted_tool();
    // D-039 v3: every discrete trigger fired this tick costs a small fee
    // regardless of whether it produced a real effect. This breaks the
    // "cheap noop trigger" local optimum (sigmoid/categorical settling on
    // craft_spear or wear_clothes because they're free no-ops when the
    // inventory is empty). NOOP (no trigger) is unaffected.
    // Accumulates into step_.vital so the schema stays at 11 components.
    void   event_action_cost(float cost = 0.001f);
    // S3.10 milestone: fires once per "first" of a kind (first house,
    // first crop, first follower) plus stage unlocks. D-043 P1: optional
    // `scale` parameter ∈ [0, 1] dampens the milestone when the agent is
    // in vital crisis (eg. about to die of thirst). Default 1.0 = full.
    void   event_milestone(float scale = 1.0f);
    // D-128b: small once-per-episode reward for the FIRST spear blow that
    // lands on a rabbit/deer. Supplies the missing gradient bridging
    // "approach prey" -> "kill" (the v-sweep proved the agent reaches a
    // resting prey at 0m but never fires there). Routed to the milestone
    // bucket so it never masquerades as a real kill in logs. Prey-only, so
    // unlike the reverted D-064 per-hit reward it cannot be farmed on fish.
    void   event_prey_blow(float v);
    // D-131: small per-NEW-map-cell reward (count-based coverage bonus). The
    // whole-life trace (D-130) showed the BC-imprinted settler stays in its
    // spawn corner forever, so prey (clustered far across the 1024 world,
    // mostly beyond the 60m vision) are never even seen. This is an
    // INSTRUMENTAL lure to walk the world: paid once per never-before-visited
    // grid cell THIS life, so it cannot be farmed by pacing (old cells pay 0)
    // and self-fades once the map is covered -> the agent reverts to survival/
    // hunting instead of exploring for its own sake. Routed to milestone bucket
    // and env-gated (DP_EXPLORE_REW, default 0 = shipped landscape unchanged).
    void   event_explore(float v);
    // D-043 P2: per-step "state-bonus" reward. Pass Φ(s_now), the
    // hand-crafted potential of the agent's current state (has_house +
    // has_clothes + n_followers + inventory_diversity). Bonus added each
    // step = state_bonus_coef × phi. Yields ~+0.0075/step at full Φ.
    void   event_state_bonus(float phi);

    // Close the step. Pushes per-component into ep_, returns total.
    float  end_step();

    const StepReward&   last_step()  const { return last_; }
    const EpisodeStats& episode()    const { return ep_; }
    void                reset_episode() { ep_.reset(); }
    const Weights&      weights()    const { return w_; }

    // D-123 SF: the UN-weighted feature vector φ for the LAST closed step
    // (snapshotted in end_step, parallel to last_step()). Length F.
    const std::array<float, F>& last_features() const { return last_feat_; }

    // D-123 SF: the weight vector w, flattened from Weights in the same order
    // as the φ channels above. dot(last_features(), weight_vector()) reproduces
    // last_step().total() (self-tested). Blended buckets (idx 6,7) carry w=1.
    std::array<float, F> weight_vector() const {
        return { w_.alive_step,    // 0 alive
                 w_.food_coef,     // 1 food
                 w_.water_coef,    // 2 water
                 w_.wolf_killed,   // 3 kills
                 w_.wolf_bite_dmg, // 4 bites (negative)
                 w_.death,         // 5 death
                 1.0f,             // 6 vital  (blended, stored weighted)
                 1.0f,             // 7 collect (blended, stored weighted)
                 w_.built_shelter, // 8 shelter
                 w_.crafted_tool,  // 9 craft
                 w_.milestone,     // 10 milestone
                 w_.state_bonus_coef }; // 11 potential
    }

private:
    Weights      w_;
    StepReward   step_{};
    StepReward   last_{};
    EpisodeStats ep_{};
    // D-123 SF: parallel un-weighted φ accumulator (current step) + snapshot.
    std::array<float, F> feat_{};
    std::array<float, F> last_feat_{};
};

}  // namespace dp::agent
