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
    int   deaths_cold = 0;   // D-112 diag (read-only): deaths via temperature extreme
    int   deaths_food = 0;   // D-112 diag (read-only): deaths via hunger/thirst == 0
    // --- decision_blueprint_nutrition (round_21): dual nutrition bars ---
    // Independent of hunger/thirst. protein refilled ONLY by eating meat
    // (hunt kills); vitamin refilled ONLY by foraging plants/crops. Both
    // decay continuously when VitalConfig.nutrition_enabled. Below
    // nutri_debuff_thresh the agent is weakened (slower, see
    // nutrition_speed_mult). At 0 (for nutri_death_grace seconds) the agent
    // dies of a malnutrition cause, tracked separately from food/cold so the
    // gate can read why a death happened. When nutrition is OFF the bars stay
    // at 100 and contribute nothing (no decay, no pressure, no debuff, no
    // death) so old-world checkpoints (bc_v9, c1) behave identically.
    float protein     = 100.0f;
    float vitamin     = 100.0f;
    int   deaths_protein = 0; // deaths via protein bar == 0 (no meat)
    int   deaths_vitamin = 0; // deaths via vitamin bar == 0 (no foraging)
    float protein_zero_s = 0.0f; // accumulated seconds protein has sat at 0
    float vitamin_zero_s = 0.0f; // accumulated seconds vitamin has sat at 0
    // --- Stage W1 world coherence (weather): how wet the body is, [0,1]. ---
    // Rises while standing in the rain unsheltered, dries off under cover /
    // over time. Updated by Environment (it knows sky + shelter + biome); fed
    // back into tick() as wet_cold_mult so a soaked agent loses body heat
    // faster. 0 when weather is disabled, so the old world is unaffected.
    float wetness        = 0.0f;
};

struct VitalConfig {
    // D-048 blueprint回归: thirst/hunger decay restored to 12/min so the
    // 5-min episode actually risks drying the agent out (60 of 100 in one
    // episode without drinking) - blueprint line 60 says "口渴 0 死亡",
    // and at 8/min an episode only burns 24, leaving thirst at 76 forever
    // with no incentive to seek water. Earlier D-040/D-041 reduced this
    // because PPO without clip+wolf_killed=0 was over-grinding drink for
    // reward; with D-045/D-047 those carrots are fixed and we can put the
    // pressure back.
    // D-053: blueprint回归。D-040~D-050 used 8/min as a "safe middle" but
    // a 5-min episode then only burns thirst 100→60, so the agent never
    // gets thirsty enough to die and PPO eventually unlearns drink (see
    // D-052 final last-120 ep_water=0). 12/min makes thirst 100→40 in
    // one episode - still survivable, but real survival pressure builds
    // up across consecutive episodes (vital state persists in PPO's
    // value function via the GRU). hunger paired so neither resource
    // dominates.
    // Per-minute base decay
    float hunger_decay_per_min = 12.0f;
    // D-040 → D-041 evolution:
    //   12.0/min: drink dominant (r_water +1.8/ep) → agent locks into drinking
    //   4.0/min:  drink dies (r_water 0) → agent loses spatial nav cue → NOOP 52% / death 40%
    //   8.0/min:  middle ground (r_water +1.2/ep, ~half of food magnitude).
    // Combined with D-041 satiation scaling in Environment.cpp (drink reward
    // tapers as thirst rises) and per-episode first_X milestones, this lets
    // PPO chase 8 main-line rewards instead of locking on a single one.
    float thirst_decay_per_min = 12.0f;
    // Stamina dynamics (per second)
    float stamina_run_decay   = 8.0f;   // when speed > run_threshold
    float stamina_walk_decay  = 0.5f;   // when moving normally
    float stamina_recover     = 4.0f;   // when nearly still
    float run_speed_threshold = 4.0f;   // m/s
    // S5 world realism: climbing steep terrain costs extra stamina. slope01
    // in [0,1] (= |grad h| rise/run, capped at 1.0 ~= 45 deg). At slope01=1
    // the per-step stamina drain WHILE MOVING is multiplied by
    // (1 + stamina_slope_mult). Flat ground (slope01=0) is unchanged, so
    // old-world checkpoints behave identically until they meet real relief.
    float stamina_slope_mult  = 3.0f;
    // Temperature: cold at night, hot in desert daytime, neutral elsewhere
    // D-094: reverted to 18/min after D-093's 50/min experiment confirmed
    // that lethality alone isn't enough - the agent just dies faster while
    // still failing to settle. The real fix is structural (removing the
    // auto-Shed respawn) so the agent has ONE shelter to anchor to. 18/min
    // keeps cold a meaningful pressure (50->-4 over a 3 min night for an
    // unsheltered agent) while leaving enough survival headroom that PPO
    // can experiment with shelter behaviour without dying off-policy in 60s.
    float temp_cold_decay_per_min = 18.0f;  // night drift toward 0
    float temp_hot_decay_per_min  = 4.0f;   // desert daytime drift toward 100
    float temp_neutral_recover    = 3.0f;   // recover toward 50 in mild conditions

    // --- decision_blueprint_nutrition (round_21) ---------------------------
    // Master switch. Default OFF: bars never decay, so VitalState's 100/100
    // is permanent and the new world is invisible to old-world runs.
    bool  nutrition_enabled        = false;
    // Per-minute base decay for each bar (independent of hunger/thirst).
    float protein_decay_per_min    = 20.0f;
    float vitamin_decay_per_min    = 20.0f;
    // Below this fraction of full (0..1) the bar's deficiency weakens the
    // agent (movement debuff, see nutrition_speed_mult). 0.30 = "<30%".
    float nutri_debuff_thresh      = 0.30f;
    // Lowest speed multiplier a fully-empty bar can impose (linear from 1.0
    // at the debuff threshold down to this floor at 0). Two empty bars
    // multiply, so the combined floor is floor*floor.
    float nutri_debuff_floor       = 0.5f;
    // Seconds a bar may sit at exactly 0 before it kills the agent. A small
    // grace prevents single-tick thrash-deaths and gives the policy a window
    // to eat. 0 = die the instant the bar empties.
    float nutri_death_grace_s      = 8.0f;
    // Amount each meat eat / forage restores to the respective bar.
    float protein_per_meat         = 35.0f;
    float vitamin_per_forage       = 30.0f;
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
    // `warmth` (default 0.0, D-112) is an ACTIVE heat-source rate in
    // temperature-points-per-MINUTE supplied by a nearby lit campfire
    // (world::FireSystem::warm_rate_at). When >0 the body temperature is
    // pulled up toward a warm target inside this same tick, BEFORE the death
    // check, so standing by a fire genuinely prevents freezing rather than
    // being a post-hoc correction. Parallel to cold_resist, but additive
    // (a source) rather than multiplicative (a damper).
    void tick(VitalState& s,
              const dp::world::World& world,
              float wx, float wy,
              float agent_speed,
              const dp::world::Daylight& day,
              float dt,
              float cold_resist = 0.0f,
              float warmth      = 0.0f,
              float slope01     = 0.0f,
              float wet_cold_mult = 1.0f) const;

    // Consumption events. Both return the *actual* amount of vital
    // restored (0 when the meter was already full); callers pass that
    // delta to RewardSystem so reward shaping reflects what the agent
    // actually got, not the attempted amount. This is the fix for the
    // S4-pre training run where PPO discovered it could spam Q in a
    // river to farm reward despite thirst already being capped at 100.
    float eat(VitalState& s, float nutrition) const;       // food refills hunger
    float drink(VitalState& s, float amount) const;        // water refills thirst
    void  rest(VitalState& s, float dt) const;             // sit still, regen stamina

    // decision_blueprint_nutrition (round_21): meat refills protein, forage
    // refills vitamin. Both clamp to [0,100] and return the actual amount
    // restored. No-op (returns 0) when nutrition is disabled. Eating also
    // clears the at-zero grace timer for that bar.
    float eat_protein(VitalState& s, float amount) const;
    float eat_vitamin(VitalState& s, float amount) const;

    // Combined movement-speed multiplier from nutrition deficiency, in
    // [floor*floor, 1]. 1.0 when both bars are above the debuff threshold (or
    // nutrition is disabled). Multiplies with the stamina speed multiplier in
    // Environment so a malnourished agent moves slower. This is a physiological
    // consequence, NOT a reward.
    float nutrition_speed_mult(const VitalState& s) const;

    // Damage / kill / respawn helpers
    void respawn(VitalState& s) const;

    const VitalConfig& config() const { return cfg_; }

private:
    VitalConfig cfg_;
};

}  // namespace dp::agent
