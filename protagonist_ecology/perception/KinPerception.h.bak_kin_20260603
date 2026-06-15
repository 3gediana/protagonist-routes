#pragma once

#include "core/interfaces/IPerception.h"

namespace neuro::routes::protagonist {

// Phase D (one-pot unified bloodline): kinship recognition channel. RESERVED
// placeholder — the dims are part of the locked unified input encoding so that
// Phase D logic can be switched on later WITHOUT changing input_dim (no lineage
// reset). Until the real logic lands, sense() writes zeros, so the brain simply
// ignores these inputs (same pattern as TaskContextPerception's reserved
// onehots).
//   [0] nearest-kin relatedness (0..1)
//   [1..2] direction to nearest kin (unit dx, dy)
class KinPerception final : public IPerception {
public:
    static constexpr std::size_t kDim = 3;

    std::size_t dim() const override { return kDim; }
    void sense(const IAgent& self, const IWorld& world, std::span<float> out) const override;
    const char* name() const override { return "kin"; }
};

}  // namespace neuro::routes::protagonist
