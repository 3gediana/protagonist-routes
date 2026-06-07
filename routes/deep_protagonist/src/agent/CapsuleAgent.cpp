#include "agent/CapsuleAgent.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace dp::agent {

namespace {
// D-134 HUNT_V2 (env-gated, default OFF -> world byte-identical). Two-phase
// locomotion: the continuous move vector's length [0,1] now selects a speed
// BAND instead of linearly scaling one top speed -- push <= WALK_PUSH walks
// (up to move_speed_), push > WALK_PUSH sprints (move_speed_ -> SPRINT_SPEED).
// Sprinting exceeds VitalSystem::run_speed_threshold (4 m/s) so it already
// drains stamina_run_decay and gets throttled back when exhausted -- the
// policy decides when to spend that budget purely via push magnitude, so no
// new action channel is added and action_dim (hence checkpoint resume) is
// unchanged.
struct HuntV2Cfg {
    bool  on;
    float walk_push;
    float sprint_speed;
};
const HuntV2Cfg& hunt_v2() {
    static const HuntV2Cfg c = []{
        const char* v = std::getenv("DP_HUNT_V2");
        bool on = v && *v && std::atoi(v) != 0;
        const char* wp = std::getenv("DP_HUNT_V2_WALKPUSH");
        const char* ss = std::getenv("DP_HUNT_V2_SPRINT");
        return HuntV2Cfg{ on,
            (wp && *wp) ? float(std::atof(wp)) : 0.6f,
            (ss && *ss) ? float(std::atof(ss)) : 6.0f };
    }();
    return c;
}
}  // namespace

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

    // D-134 HUNT_V2: remap the push magnitude to a two-band speed (walk vs
    // sprint). Direction is preserved; the band is chosen by |push|, so the
    // policy gains a sprint gear without any new action channel. Default OFF
    // keeps the legacy linear |push| * move_speed_ mapping below.
    const auto& hv2 = hunt_v2();
    if (hv2.on && state_.on_ground) {
        float m = std::sqrt(mx*mx + my*my);          // already clamped to [0,1]
        if (m > 1e-4f) {
            float band_speed;
            if (m <= hv2.walk_push) {
                band_speed = (m / hv2.walk_push) * move_speed_;          // 0..walk
            } else {
                float t = (m - hv2.walk_push) / (1.0f - hv2.walk_push);   // 0..1
                band_speed = move_speed_
                           + t * (hv2.sprint_speed - move_speed_);        // walk..sprint
            }
            band_speed *= speed_mult;
            float inv = band_speed / m;              // dir(unit) * band_speed
            state_.vel_x = mx * inv;
            state_.vel_y = my * inv;
        } else {
            state_.vel_x = 0.0f;
            state_.vel_y = 0.0f;
        }
    } else if (state_.on_ground) {
        state_.vel_x = mx * effective_speed;
        state_.vel_y = my * effective_speed;
    }

    phys.step(world, state_, dt);

    inside_cave_ = world.is_underground(state_.pos_x, state_.pos_y,
                                        state_.pos_z + state_.height * 0.5f);
    last_action_ = a;
}

}  // namespace dp::agent
