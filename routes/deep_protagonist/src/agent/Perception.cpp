#include "agent/Perception.hpp"

namespace dp::agent {

void Perception::compute(const CapsuleAgent& agent,
                         const dp::world::FoodSystem& food,
                         PerceptionOutput& out) const {
    const auto& s = agent.state();

    // Velocity
    out.data[0] = s.vel_x;
    out.data[1] = s.vel_y;
    out.data[2] = s.vel_z;

    // Closest K food, relative to the agent's center
    const float cx = s.pos_x;
    const float cy = s.pos_y;
    const float cz = s.pos_z + s.height * 0.5f;
    const float inv_r = 1.0f / PerceptionOutput::VISION_RANGE;

    auto idxs = food.closest_k(cx, cy, cz, PerceptionOutput::FOOD_K);
    for (int i = 0; i < PerceptionOutput::FOOD_K; ++i) {
        int slot = 3 + i * 3;
        int p = idxs[i];
        if (p < 0) {
            out.data[slot]     = 0.0f;
            out.data[slot + 1] = 0.0f;
            out.data[slot + 2] = 0.0f;
        } else {
            const auto& food_p = food.pellets()[p];
            out.data[slot]     = (food_p.x - cx) * inv_r;
            out.data[slot + 1] = (food_p.y - cy) * inv_r;
            out.data[slot + 2] = (food_p.z - cz) * inv_r;
        }
    }
}

}  // namespace dp::agent
