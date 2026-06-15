#include "world/Physics.hpp"

#include <algorithm>
#include <cmath>

namespace dp::world {

void Physics::step(const World& world, CapsuleState& s, float dt) const {
    // Sub-stepping: clamp each integration step to max_step_dt for numerical
    // stability when velocities are high (e.g. fall from sky).
    float remaining = dt;
    while (remaining > 0.0f) {
        float h = std::min(remaining, cfg_.max_step_dt);
        substep(world, s, h);
        remaining -= h;
    }
}

void Physics::substep(const World& world, CapsuleState& s, float dt) const {
    // 1) Apply gravity
    s.vel_z -= cfg_.gravity * dt;

    // 2) Integrate position (semi-implicit Euler)
    s.pos_x += s.vel_x * dt;
    s.pos_y += s.vel_y * dt;
    s.pos_z += s.vel_z * dt;

    // 3) Resolve collision against terrain or cave surfaces
    // Determine the relevant ground level under the foot.
    bool inside_cave = false;
    float floor_z = 0.0f;
    float ceiling_z = 1e9f;

    // Check if the capsule center is inside any cave volume
    float center_z = s.pos_z + s.height * 0.5f;
    for (const auto& c : world.caves().caves()) {
        if (c.contains(s.pos_x, s.pos_y, center_z)) {
            inside_cave = true;
            floor_z = c.floor_z;
            ceiling_z = c.ceiling_z;
            break;
        }
    }

    // If not inside a cave, the floor is the terrain surface
    if (!inside_cave) {
        floor_z = world.surface_height(s.pos_x, s.pos_y);
    }

    // Foot below floor -> snap up and zero vertical velocity
    if (s.pos_z < floor_z) {
        s.pos_z = floor_z;
        if (s.vel_z < 0.0f) s.vel_z = 0.0f;
        s.on_ground = true;
    } else if (std::abs(s.pos_z - floor_z) < 0.05f) {
        s.on_ground = true;
    } else {
        s.on_ground = false;
    }

    // Head above ceiling -> snap down and zero vertical velocity
    float head_z = s.pos_z + s.height;
    if (head_z > ceiling_z) {
        s.pos_z = ceiling_z - s.height;
        if (s.vel_z > 0.0f) s.vel_z = 0.0f;
    }

    // 4) Friction / air drag (horizontal only)
    float decay = s.on_ground ? cfg_.ground_friction : cfg_.air_drag;
    float scale = std::max(0.0f, 1.0f - decay * dt);
    s.vel_x *= scale;
    s.vel_y *= scale;

    // 5) Keep capsule inside world bounds (clamp to heightmap extents)
    float wx_max = world.heightmap().world_size_x();
    float wy_max = world.heightmap().world_size_y();
    s.pos_x = std::clamp(s.pos_x, 0.0f, wx_max);
    s.pos_y = std::clamp(s.pos_y, 0.0f, wy_max);
}

}  // namespace dp::world
