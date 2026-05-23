#pragma once

#include <cstdint>

namespace dp::agent {

// One tick worth of "what does the agent want to do".
//
// This is the single contract between any controller (hand-coded wander,
// keyboard input, or PPO policy) and CapsuleAgent. Keeping it tight makes
// it cheap to feed PPO outputs straight into apply_action() in S4.
//
// Continuous channels are clamped by CapsuleAgent::apply_action; callers do
// not need to clip themselves.
struct AgentAction {
    // ---- Continuous channels ----------------------------------------------
    // Desired horizontal move direction in world XY plane. (move_x, move_y)
    // is treated as a 2D vector: its length encodes "how hard to push" in
    // [0, 1] (anything > 1 gets normalized down). 0 = stand still.
    float move_x = 0.0f;
    float move_y = 0.0f;

    // Yaw rotation rate in rad/s. Positive = counter-clockwise (looking
    // down from +Z). Used for steering / camera headings.
    float yaw_rate = 0.0f;

    // ---- Discrete one-tick triggers ---------------------------------------
    // True = "try this action this tick"; false = ignore. Multiple flags
    // can be true at once (e.g. eat + collect on the same tick), but only
    // one resolves per system.
    bool eat            = false;  // harvest nearby ripe plants
    bool drink          = false;  // drink from river/swamp under feet
    bool attack         = false;  // attack closest hostile animal in arc
    bool collect        = false;  // pick up nearby resource (wood/stone/grass)
    bool place_shelter  = false;  // build/anchor shelter at current location
    bool craft_spear    = false;  // combine inventory items into spear
    bool sleep          = false;  // rest at shelter (recovers stamina)
};

// Convenience: a no-op action (the default) means "stand still, do nothing".
inline AgentAction noop_action() { return AgentAction{}; }

}  // namespace dp::agent
