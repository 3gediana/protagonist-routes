#pragma once

#include "core/interfaces/IAgentCapability.h"

#include <cstddef>

namespace neuro {

class PredatorHuntCapability final : public IAgentCapability {
public:
    PredatorHuntCapability(float hunt_radius, float damage, float energy_gain, float cost);

    std::size_t consumedOutputs() const override;
    void apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) override;
    const char* name() const override;

private:
    float hunt_radius_;
    float damage_;
    float energy_gain_;
    float cost_;
};

}  // namespace neuro
