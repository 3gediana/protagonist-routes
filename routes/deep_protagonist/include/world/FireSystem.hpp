#pragma once

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

    // Temperature recovery rate (points per MINUTE) at (px,py): warm_rate if a
    // LIT fire is within warm_radius, else 0. Fed into VitalSystem::tick as the
    // heat-source term (parallel to cold_resist).
    float warm_rate_at(float px, float py) const;

    int count_lit() const;

    const std::vector<Campfire>& fires() const { return fires_; }
    const Config& config() const { return cfg_; }

    void clear();  // episode reset

private:
    Config                cfg_;
    std::vector<Campfire> fires_;
};

}  // namespace dp::world
