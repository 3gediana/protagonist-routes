#pragma once

#include <functional>
#include <vector>

namespace dp::world {

// D-112 (Stage3 Level-2, user mandate "必须加烤火"): a campfire is a placed
// world entity (NOT an inventory item, so it sidesteps the CAPACITY==3 block
// that made clothing infeasible) and an ACTIVE heat source: while a lit,
// fuelled fire is within warm_radius, VitalSystem pulls the agent's body
// temperature up toward a warm target. This directly attacks the residual
// "builds a house but still freezes" death mode that clothing (resist 0.10)
// is too weak to fix.
//
// Causal, not a free button (decision_round_10 redline): building or refuelling
// a fire costs 1 Wood, and a lit fire burns its fuel down over time and
// extinguishes when fuel hits 0. No direct reward is attached to fire actions;
// the only payoff is the temperature recovery the agent earns by staying near
// a fire it built. Behaviour is taught purely via oracle demo + DAgger.
struct Campfire {
    float x    = 0.0f;
    float y    = 0.0f;
    float fuel = 0.0f;   // remaining burn time, in seconds
    bool  lit  = false;
};

class FireSystem {
public:
    struct Config {
        float warm_radius      = 6.0f;    // metres within which a fire warms
        float fuel_per_wood    = 90.0f;   // seconds of burn added per Wood
        float fuel_max         = 180.0f;  // cap so refuel can't hoard infinitely
        float warm_rate_per_min = 45.0f;  // temp pts/min recovery near a fire
        float merge_radius     = 3.0f;    // refuel an existing fire within this
        int   max_fires        = 8;       // hard cap on simultaneous campfires
    };

    explicit FireSystem(const Config& cfg = {}) : cfg_(cfg) {}

    // Agent pressed tend_fire at (x,y) and has >=1 Wood available. If a
    // campfire is within merge_radius -> refuel it (up to fuel_max); else
    // build a new lit campfire at (x,y) with a fresh fuel load. Returns true
    // iff the action did something useful (caller then consumes 1 Wood);
    // false on a no-op (fire already full, or fire cap reached) so no Wood is
    // wasted.
    bool tend(float x, float y);

    // Per-frame: burn lit fires' fuel down; extinguish + remove any that hit 0.
    void tick(float dt);

    // Stage W1 (decision_blueprint_world_coherence, user "下雨要能浇灭火"):
    // rain pulls a lit fire's fuel down FASTER, so an unattended fire goes out
    // in the rain unless it is under a roof. `douse_per_s` is the EXTRA fuel
    // (seconds of burn) consumed per real second while a fire is exposed, on
    // top of the normal burn in tick(). `exposed(x,y)` is supplied by the
    // caller (Environment) and returns true iff the fire at (x,y) is being
    // rained on (sky raining AND not roofed by a building/shelter AND the
    // local biome actually gets rain). Causal, not a button: the only effect
    // is faster fuel loss → the agent must re-light or protect the fire. No
    // reward is involved. Call this BEFORE tick(dt) so tick() then removes any
    // fire this doused to 0. No-op when douse_per_s<=0.
    void apply_rain(float dt, float douse_per_s,
                    const std::function<bool(float, float)>& exposed);

    // Temperature recovery rate (points per MINUTE) at (px,py): warm_rate if a
    // LIT fire is within warm_radius, else 0. Fed into VitalSystem::tick as the
    // heat-source term (parallel to cold_resist).
    float warm_rate_at(float px, float py) const;

    // D-122 RUNG 1 (cook): burn `amount` seconds of fuel off the nearest LIT
    // fire within `radius` of (px,py). Returns true iff such a fire existed
    // (and the fuel was charged). This is the CAUSAL COST of cooking: each
    // cook action spends extra fuel, so the agent must keep feeding the fire
    // Wood (tend_fire) or it goes out — cooking is never free. No reward is
    // attached. The fire may drop to 0 fuel and be extinguished by the next
    // tick(), exactly as if it had burned down naturally.
    bool consume_fuel_near(float px, float py, float radius, float amount);

    int count_lit() const;

    const std::vector<Campfire>& fires() const { return fires_; }
    const Config& config() const { return cfg_; }

    void clear();  // episode reset

private:
    Config                cfg_;
    std::vector<Campfire> fires_;
};

}  // namespace dp::world
