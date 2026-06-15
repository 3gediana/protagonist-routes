#pragma once

#include "core/interfaces/IPerception.h"

namespace neuro::routes::protagonist {

// Devin (2026-06-10): world-border proprioception. The map edges are hard
// clamps the agent otherwise cannot sense at all, which is why untrained
// protagonists pile up along walls and corners. This exposes:
//   [0] 2*x/W - 1   normalized horizontal position in [-1, 1]
//   [1] 2*y/H - 1   normalized vertical position in [-1, 1]
//   [2] closeness ramp to nearest vertical wall (0 far -> 1 touching)
//   [3] closeness ramp to nearest horizontal wall (0 far -> 1 touching)
//
// Enabling this adds kDim input dims, which changes the protagonist genome
// input_dim, so it starts a FRESH lineage (gated by
// protagonist_border_perception_enabled, default false -> existing checkpoints
// stay loadable).
class BorderPerception final : public IPerception {
public:
    static constexpr std::size_t kDim = 4;

    std::size_t dim() const override { return kDim; }
    void sense(const IAgent& self, const IWorld& world, std::span<float> out) const override;
    const char* name() const override { return "border"; }
};

}  // namespace neuro::routes::protagonist
