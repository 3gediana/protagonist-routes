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

    // Tunables
    float fruit_regrow_seconds  = 60.0f;
    float berry_regrow_seconds  = 30.0f;
    float cactus_regrow_seconds = 90.0f;
    float mushroom_regrow_seconds = 120.0f;

private:
    std::vector<PlantInstance> plants_;
    uint64_t rng_state_ = 1;

    float frand();
    float regrow_for(PlantKind k) const;
};

}  // namespace dp::world
