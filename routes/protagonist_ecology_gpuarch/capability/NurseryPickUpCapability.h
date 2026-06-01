#pragma once

#include "core/interfaces/IAgentCapability.h"

namespace neuro::routes::protagonist {

class NurseryPickUpCapability final : public IAgentCapability {
public:
    NurseryPickUpCapability(float pick_radius, float energy_cost);

    std::size_t consumedOutputs() const override { return 1; }
    void apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) override;
    const char* name() const override { return "nursery_pick_up"; }

private:
    float pick_radius_;
    float energy_cost_;
};

}  // namespace neuro::routes::protagonist
