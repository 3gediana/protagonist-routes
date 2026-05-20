#pragma once

#include "core/interfaces/IPerception.h"

#include <cstddef>

namespace neuro {

class NearbyObjectPerception final : public IPerception {
public:
    static constexpr std::size_t kDim = 4;  // dx, dy, distance, type_onehot(1)

    explicit NearbyObjectPerception(float perception_radius);

    std::size_t dim() const override { return kDim; }
    void sense(const IAgent& agent, const IWorld& world, std::span<float> inputs) const override;
    const char* name() const override { return "nearby_object"; }

private:
    float perception_radius_;
};

}  // namespace neuro
