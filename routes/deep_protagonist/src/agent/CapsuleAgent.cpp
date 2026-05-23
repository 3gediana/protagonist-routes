#include "agent/CapsuleAgent.hpp"

#include <algorithm>
#include <cmath>

namespace dp::agent {

CapsuleAgent::CapsuleAgent(uint64_t seed) : rng_state_(seed ? seed : 1) {
    state_.radius = 0.4f;
    state_.height = 1.6f;
}

void CapsuleAgent::reset(float wx, float wy, float wz) {
    state_.pos_x = wx;
    state_.pos_y = wy;
    state_.pos_z = wz;
    state_.vel_x = 0.0f;
    state_.vel_y = 0.0f;
    state_.vel_z = 0.0f;
    state_.on_ground = false;
    yaw_ = 0.0f;
    wander_timer_ = 0.0f;
    last_action_ = AgentAction{};
}

float CapsuleAgent::frand() {
    rng_state_ ^= rng_state_ >> 12;
    rng_state_ ^= rng_state_ << 25;
    rng_state_ ^= rng_state_ >> 27;
    uint64_t r = rng_state_ * 0x2545F4914F6CDD1DULL;
    return static_cast<float>(r & 0xFFFFFFu) / 16777216.0f;
}

AgentAction CapsuleAgent::sample_wander_action(float dt) {
    wander_timer_ -= dt;
    if (wander_timer_ <= 0.0f) {
        float angle = frand() * 6.2831853f;
        wander_x_ = std::cos(angle);
        wander_y_ = std::sin(angle);
        wander_timer_ = wander_change_period_;
    }
    AgentAction a;
    a.move_x = wander_x_;  // unit length already
    a.move_y = wander_y_;
    return a;
}

void CapsuleAgent::apply_action(const AgentAction& a,
                                const dp::world::World& world,
                                const dp::world::Physics& phys,
                                float dt) {
    // Normalise move vector if it exceeds unit length. Length in [0, 1] is
    // treated as "how hard to push" (so a controller can choose to stand
    // still by emitting (0, 0)).
    float mx = a.move_x;
    float my = a.move_y;
    float ml2 = mx*mx + my*my;
    if (ml2 > 1.0f) {
        float k = 1.0f / std::sqrt(ml2);
        mx *= k;
        my *= k;
    }

    // Update yaw. yaw_rate is in rad/s.
    yaw_ += a.yaw_rate * dt;
    // Keep yaw in [-pi, pi] for stability of downstream consumers.
    while (yaw_ >  3.14159265f) yaw_ -= 6.2831853f;
    while (yaw_ < -3.14159265f) yaw_ += 6.2831853f;

    // Biome-modulated max speed. Swamp halves the speed (blueprint
    // line 20: "沼泽减速"). Other biomes ride at the base value.
    float speed_mult = 1.0f;
    auto biome = world.biome_at(state_.pos_x, state_.pos_y);
    if (biome == dp::world::Biome::Swamp) {
        speed_mult = 0.5f;
    }
    float effective_speed = move_speed_ * speed_mult;

    // Drive horizontal velocity toward action target (ground only).
    if (state_.on_ground) {
        state_.vel_x = mx * effective_speed;
        state_.vel_y = my * effective_speed;
    }

    phys.step(world, state_, dt);

    inside_cave_ = world.is_underground(state_.pos_x, state_.pos_y,
                                        state_.pos_z + state_.height * 0.5f);
    last_action_ = a;
}

}  // namespace dp::agent
