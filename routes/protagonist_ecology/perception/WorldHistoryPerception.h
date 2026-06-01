#pragma once

#include "core/interfaces/IPerception.h"

namespace neuro::routes::protagonist {

// Phase F (one-pot unified bloodline): persistent world-history channel (what
// the agent's lineage has durably changed about the world). RESERVED
// placeholder — dims are part of the locked unified input encoding so Phase F
// logic can be switched on later WITHOUT changing input_dim. sense() writes
// zeros until the real logic lands.
//   [0] local accumulated-development level (0..1)
//   [1] time since last visited / world age signal (0..1)
class WorldHistoryPerception final : public IPerception {
public:
    static constexpr std::size_t kDim = 2;

    std::size_t dim() const override { return kDim; }
    void sense(const IAgent& self, const IWorld& world, std::span<float> out) const override;
    const char* name() const override { return "world_history"; }
};

}  // namespace neuro::routes::protagonist
