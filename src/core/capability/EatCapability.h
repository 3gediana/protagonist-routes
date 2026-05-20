#pragma once

#include "core/interfaces/IAgentCapability.h"

namespace neuro {

class EatCapability final : public IAgentCapability {
public:
    explicit EatCapability(float eat_radius, float eat_rate_per_second = 18.0f);

    std::size_t consumedOutputs() const override;
    void apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) override;
    const char* name() const override;

private:
    float eat_radius_;
    float eat_rate_per_second_;
};

}  // namespace neuro
