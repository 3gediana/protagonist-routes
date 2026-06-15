#pragma once

#include "world/Decorations.hpp"
#include "world/World.hpp"

#include <cstdint>
#include <vector>

namespace dp::world {

// Each plant kind has different food/water value and gameplay rules.
//   Berry  - small bushes on plains, easy to find, low calories
//   Fruit  - hangs from forest trees, mid calories
//   Cactus - desert, less food but ALSO refills thirst (water plant)
//   Mushroom - swamp, occasional poison (negative effect 30% of the time)
enum class PlantKind : uint8_t {
    Berry    = 0,
    Fruit    = 1,
    Cactus   = 2,
    Mushroom = 3,
};

const char* plant_kind_name(PlantKind k);

struct PlantInstance {
    float x, y, z;
    PlantKind kind;
    bool  ripe          = true;     // true = harvestable
    float regrow_timer  = 0.0f;     // seconds remaining to ripen again
};

// PlantSystem turns Decorations into harvestable resources. Forest trees grow
// fruit, plains/forest get berry bushes scattered, deserts get cactus-fruit,
// swamps get mushrooms. Once harvested, a plant takes a configurable cooldown
// to ripen again.
class PlantSystem {
public:
    void initialize(uint64_t seed, const World& world,
                    const Decorations& decorations);

    void tick(float dt);
    // D-132 forage scarcity: same regrow logic, but also drives the periodic
    // global redistribution (needs the agent position to push the new field
    // away from where the agent currently sits). When scarcity is disabled
    // this is identical to tick(dt).
    void tick(float dt, float agent_x, float agent_y);

    // Try to harvest any ripe plant within `radius` of (x,y,z). Returns the
    // KIND of plant harvested for each pellet eaten this tick. Multiple
    // plants can be harvested per call if they're within range.
    // D-092: optional is_night flag. When true, the call returns nothing
    // even if ripe plants are in range - the blueprint's 夜里采不到野果
    // (no wild forage at night) rule. Existing callers that don't pass
    // is_night keep the old "always harvestable" behaviour. The visual
    // plant placement is unchanged; only harvest is blocked.
    std::vector<PlantKind> try_harvest(float x, float y, float z,
                                       float radius = 1.5f,
                                       bool  is_night = false);

    const std::vector<PlantInstance>& plants() const { return plants_; }

    // ---- D-132 forage scarcity (env-gated; default disabled = world unchanged)
    // Instead of giving an exploration *reward* (which Goodharts into
    // wandering), we make the WORLD itself scarce near the agent so leaving
    // the spawn corner becomes instrumentally necessary to keep eating:
    //   - barren spawn: no ripe forage within `barren_r` of the spawn point
    //   - far initial placement: all plants relocated outside that radius
    //   - vicinity depletes: a harvested plant respawns far away (not in place)
    //   - non-stationary: every `redist_sec` the whole forage field is
    //     re-scattered to fresh random land, biased away from the agent
    // Only forageable plants move; animals/fish/water are untouched.
    struct ScarcityCfg {
        bool  enabled    = false;
        float barren_r   = 90.0f;   // ripe forage kept out of this radius
        float redist_sec = 90.0f;   // global redistribution period (0 = never)
        float spawn_x    = 0.0f;
        float spawn_y    = 0.0f;
    };
    void configure_scarcity(const ScarcityCfg& c, const World* world);
    // D-133: when the spawn is randomised per episode, move the barren ring's
    // centre to follow it. No-op fields if scarcity is disabled.
    void set_scarcity_spawn(float x, float y) { scar_.spawn_x = x; scar_.spawn_y = y; }
    // Relocate every plant to random land outside `barren_r` of spawn. Call
    // once after the spawn point is chosen and again at each episode reset so
    // every life starts with a barren spawn vicinity. No-op if disabled.
    void apply_initial_scarcity();
    // Distance from (x,y) to the nearest ripe plant (for logging/metrics).
    float nearest_ripe_dist(float x, float y) const;

    // Tunables
    float fruit_regrow_seconds  = 60.0f;
    float berry_regrow_seconds  = 30.0f;
    float cactus_regrow_seconds = 90.0f;
    float mushroom_regrow_seconds = 120.0f;

private:
    std::vector<PlantInstance> plants_;
    uint64_t rng_state_ = 1;

    // D-132 forage scarcity state
    ScarcityCfg  scar_;
    const World* scar_world_ = nullptr;
    float        redist_timer_ = 0.0f;
    // Move one plant to a random dry-land surface spot that is > barren_r from
    // spawn AND > avoid_r from (avoid_x, avoid_y). Preserves the plant kind.
    void relocate_plant_(PlantInstance& p,
                         float avoid_x, float avoid_y, float avoid_r);

    float frand();
    float regrow_for(PlantKind k) const;
};

}  // namespace dp::world
