#pragma once

#include "world/World.hpp"

namespace dp::world {

// Minimal physics: a capsule (vertical, axis-aligned) integrating against a
// heightfield. Caves are treated as "underground volumes" - if the agent's
// foot is inside a cave box, the cave's floor/ceiling become its collision
// surfaces instead of the heightmap.
//
// State convention: `pos` is the foot position of the capsule, `vel` is the
// linear velocity in world meters/second.
struct CapsuleState {
    float pos_x = 0.0f;
    float pos_y = 0.0f;
    float pos_z = 0.0f;       // foot height (terrain-relative once on ground)
    float vel_x = 0.0f;
    float vel_y = 0.0f;
    float vel_z = 0.0f;
    float radius = 0.4f;      // capsule radius
    float height = 1.6f;      // total capsule height (foot to head)
    bool  on_ground = false;
};

class Physics {
public:
    struct Config {
        float gravity = 9.8f;            // m/s^2
        float ground_friction = 6.0f;    // velocity decay when grounded
        float air_drag = 0.05f;          // velocity decay when airborne
        float max_step_dt = 0.02f;       // sub-step cap (s) for stability
    };

    explicit Physics(const Config& cfg = {}) : cfg_(cfg) {}

    // Advance the capsule by `dt` seconds against the world.
    void step(const World& world, CapsuleState& s, float dt) const;

private:
    Config cfg_;

    void substep(const World& world, CapsuleState& s, float dt) const;
};

}  // namespace dp::world
