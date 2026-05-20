#pragma once

#include "core/interfaces/IAgentCapability.h"

#include <cstddef>

namespace neuro {

class PickUpCapability final : public IAgentCapability {
public:
    explicit PickUpCapability(float pick_radius, float energy_cost);

    std::size_t consumedOutputs() const override { return 1; }
    void apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) override;
    const char* name() const override;

private:
    float pick_radius_;
    float energy_cost_;
};

}  // namespace neuro
