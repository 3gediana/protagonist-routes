#pragma once

#include "world/World.hpp"

#include <cstdint>
#include <vector>

namespace dp::world {

// A small population of "food" pellets scattered on the terrain surface.
// When eaten, a pellet respawns at a fresh random location to keep the
// total alive count constant. Underground food spawning is disabled - food
// only sits on top of the heightmap.
//
// This is the simplest possible reward source for S2 training: agent
// touches food -> agent gets reward -> food respawns elsewhere.
class FoodSystem {
public:
    struct Pellet {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        bool  alive = true;
    };

    static constexpr float DEFAULT_RADIUS = 0.6f;

    void initialize(uint64_t seed, int target_count, const World& world);

    // Try to eat any pellet whose center is within `eat_radius` of (x,y,z).
    // Eaten pellets are immediately respawned at a new random location.
    // Returns the number of pellets eaten this call.
    int try_eat(float x, float y, float z, const World& world,
                float eat_radius = DEFAULT_RADIUS);

    // Return the indices of (up to) `k` pellets sorted by distance to (x,y,z).
    // Only alive pellets are considered. The returned indices reference
    // pellets() entries; if fewer than k are alive, the trailing slots are
    // filled with -1.
    std::vector<int> closest_k(float x, float y, float z, int k) const;

    int alive_count() const;
    const std::vector<Pellet>& pellets() const { return pellets_; }

private:
    std::vector<Pellet> pellets_;
    int target_count_ = 0;
    uint64_t rng_state_ = 1;

    void respawn_at(int idx, const World& world);
    float frand();
};

}  // namespace dp::world
