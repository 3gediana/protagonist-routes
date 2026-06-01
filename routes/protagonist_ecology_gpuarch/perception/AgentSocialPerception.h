#pragma once

#include "core/interfaces/IPerception.h"

namespace neuro::routes::protagonist {

class AgentSocialPerception final : public IPerception {
public:
    // Dimensions:
    //   [0,1] nearest prey direction (normalized dx, dy)
    //   [2]   nearest prey distance (normalized to sense_radius)
    //   [3]   nearest prey presence flag
    //   [4,5] nearest predator direction
    //   [6]   nearest predator distance (normalized)
    //   [7]   nearest predator presence flag
    //   [8]   protagonist peer count in radius (normalized, cap 8)
    //   [9]   predator count in radius (normalized, cap 8)
    // The last two dimensions enable the protagonist to decide fight-back
    // vs flee based on local social context (emergent shelter L1 follow-up,
    // 2026-05-22).
    static constexpr std::size_t kDim = 10;
    static constexpr float kCountCap = 8.0f;

    explicit AgentSocialPerception(float sense_radius) : sense_radius_(sense_radius) {}

    std::size_t dim() const override { return kDim; }
    void sense(const IAgent& self, const IWorld& world, std::span<float> out) const override;
    const char* name() const override { return "agent_social"; }

private:
    float sense_radius_;
};

}  // namespace neuro::routes::protagonist
