#pragma once

#include "core/interfaces/IPerception.h"
#include "core/types/Vec2.h"

#include <vector>

namespace neuro::routes::protagonist {

// Realism (2026-06): lets the protagonist sense caves so cave-seeking /
// sheltering becomes learnable (caves are the strongest thermal + predator
// shelter via ThermalTickCapability's cave buffer). Emits 4 channels:
//   [0] in_cave    1.0 when the agent is currently inside a cave, else 0
//   [1] dir_x      unit vector toward the nearest cave cell (0 if inside/none)
//   [2] dir_y
//   [3] dist_norm  distance to nearest cave / sense_radius, clamped to [0,1]
//                  (1.0 when no cave is known / agent already inside one)
// Cave cells are static after world generation, so their centers are cached
// on the first sense() call (the perception object lives one agent-lifetime).
class CavePerception final : public IPerception {
public:
    static constexpr std::size_t kDim = 4;

    explicit CavePerception(float sense_radius = 120.0f) : sense_radius_(sense_radius) {}

    std::size_t dim() const override { return kDim; }
    void sense(const IAgent& self, const IWorld& world, std::span<float> out) const override;
    const char* name() const override { return "cave"; }

private:
    float sense_radius_;
    mutable bool cache_built_{false};
    mutable std::vector<Vec2> cave_cells_;
};

}  // namespace neuro::routes::protagonist
