#pragma once

#include "core/interfaces/IPerception.h"

namespace neuro {

class InfectionPerception final : public IPerception {
public:
    static constexpr std::size_t kDim = 5;

    InfectionPerception(float perception_radius);

    std::size_t dim() const override { return kDim; }
    void sense(const IAgent& agent, const IWorld& world, std::span<float> output) const override;
    const char* name() const override { return "infection"; }

private:
    float perception_radius_;
};

}  // namespace neuro
