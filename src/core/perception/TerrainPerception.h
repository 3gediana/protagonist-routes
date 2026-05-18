#pragma once

#include "core/interfaces/IPerception.h"

namespace neuro {

class TerrainPerception final : public IPerception {
public:
    static constexpr std::size_t kDim = 5;

    explicit TerrainPerception(float height_scale);

    std::size_t dim() const override { return kDim; }
    void sense(const IAgent& agent, const IWorld& world, std::span<float> output) const override;
    const char* name() const override { return "terrain"; }

private:
    float height_scale_;
};

}  // namespace neuro
