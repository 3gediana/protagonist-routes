#pragma once

#include "core/interfaces/IAgentCapability.h"

namespace neuro {

class EatCapability final : public IAgentCapability {
public:
    explicit EatCapability(float eat_radius);

    std::size_t consumedOutputs() const override;
    void apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) override;
    const char* name() const override;

private:
    float eat_radius_;
};

}  // namespace neuro
