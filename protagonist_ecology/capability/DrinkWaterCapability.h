#pragma once

#include "core/interfaces/IAgentCapability.h"

namespace neuro::routes::protagonist {

class DrinkWaterCapability final : public IAgentCapability {
public:
    explicit DrinkWaterCapability(float activation_threshold = 0.5f);

    std::size_t consumedOutputs() const override { return 1; }
    void apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) override;
    const char* name() const override { return "drink_water"; }

private:
    float activation_threshold_;
};

}  // namespace neuro::routes::protagonist
