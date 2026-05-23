#pragma once

#include "agent/AgentAction.hpp"
#include "world/Physics.hpp"
#include "world/World.hpp"

namespace dp::agent {

// The agent's body. Owns physics state (position / velocity / on_ground)
// and a heading (yaw). Exposes only one *advance* primitive,
// apply_action(): every controller (hand-coded wander, keyboard input,
// PPO policy) feeds the agent through the same channel.
//
// Locomotion model (kept simple, but stable):
//   - The (move_x, move_y) vector in AgentAction is treated as a desired
//     horizontal velocity expressed as a fraction of move_speed_.
//     Length > 1 is normalized down to 1.
//   - When on_ground=true we directly slot vel_xy. Air control is disabled
//     so the agent does not "fly" out of cave overhangs.
//   - Yaw rotates by yaw_rate * dt; we don't enforce that move_dir matches
//     yaw, that's the controller's responsibility.
class CapsuleAgent {
public:
    explicit CapsuleAgent(uint64_t seed);

    void reset(float wx, float wy, float wz);

    // Apply one tick's action. Steps physics afterwards. The action's
    // discrete triggers (eat/drink/attack/...) are NOT consumed here -
    // they are read out by the outer game loop via last_action().
    void apply_action(const AgentAction& a,
                      const dp::world::World& world,
                      const dp::world::Physics& phys,
                      float dt);

    // For controllers that still want the old "random wander" behaviour.
    // Returns an action; the caller is responsible for applying it.
    AgentAction sample_wander_action(float dt);

    const dp::world::CapsuleState& state() const { return state_; }
    const AgentAction&             last_action() const { return last_action_; }
    float                          yaw() const { return yaw_; }
    bool                           inside_cave() const { return inside_cave_; }

private:
    dp::world::CapsuleState state_;
    AgentAction             last_action_;
    uint64_t                rng_state_;
    float                   yaw_ = 0.0f;          // facing (rad)
    float                   wander_x_ = 1.0f;
    float                   wander_y_ = 0.0f;
    float                   wander_timer_ = 0.0f;
    bool                    inside_cave_ = false;

    float                   move_speed_ = 3.0f;          // m/s
    float                   wander_change_period_ = 2.0f;

    float                   frand();
};

}  // namespace dp::agent
