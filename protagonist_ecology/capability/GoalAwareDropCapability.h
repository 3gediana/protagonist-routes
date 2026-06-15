#pragma once

#include "core/interfaces/IAgentCapability.h"

#include <cstddef>

namespace neuro::routes::protagonist {

class GoalAwareDropCapability final : public IAgentCapability {
public:
    explicit GoalAwareDropCapability(float energy_cost);

    std::size_t consumedOutputs() const override { return 1; }
    void apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) override;
    const char* name() const override { return "drop"; }

private:
    float energy_cost_;
};

}  // namespace neuro::routes::protagonist
