#pragma once

#include "agent/CapsuleAgent.hpp"
#include "world/FoodSystem.hpp"

#include <array>

namespace dp::agent {

// Convert agent + world state into a flat float vector that an MLP can chew.
//
// Layout (18 floats total):
//   [ 0..2 ]  agent linear velocity (vx, vy, vz), in m/s
//   [ 3..5 ]  vector to closest food #1   (dx, dy, dz), normalized to ~[-1,1]
//   [ 6..8 ]  vector to closest food #2
//   [ 9..11]  vector to closest food #3
//   [12..14]  vector to closest food #4
//   [15..17]  vector to closest food #5
//
// "Vector to food" is (food_pos - agent_pos) divided by VISION_RANGE so that
// the value is roughly in [-1, 1] across the typical world. If a slot has
// no food (e.g. fewer than 5 alive), it is filled with zeros - the policy
// will just learn to ignore zero slots.
struct PerceptionOutput {
    static constexpr int FOOD_K = 5;
    static constexpr int SIZE   = 3 + FOOD_K * 3;
    static constexpr float VISION_RANGE = 64.0f;  // m

    std::array<float, SIZE> data{};
};

class Perception {
public:
    void compute(const CapsuleAgent& agent,
                 const dp::world::FoodSystem& food,
                 PerceptionOutput& out) const;
};

}  // namespace dp::agent
