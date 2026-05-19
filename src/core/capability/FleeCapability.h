#pragma once

#include "core/interfaces/IAgentCapability.h"

namespace neuro {

class FleeCapability final : public IAgentCapability {
public:
    FleeCapability(float flee_radius, float flee_speed_boost, float flee_cost);

    std::size_t consumedOutputs() const override;
    void apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) override;
    const char* name() const override;

private:
    float flee_radius_;
    float flee_speed_boost_;
    float flee_cost_;
};

}  // namespace neuro
