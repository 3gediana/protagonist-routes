#pragma once

#include "core/interfaces/IAgentCapability.h"

#include <cstddef>

namespace neuro {

class DropCapability final : public IAgentCapability {
public:
    explicit DropCapability(float energy_cost);

    std::size_t consumedOutputs() const override { return 1; }
    void apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) override;
    const char* name() const override;

private:
    float energy_cost_;
};

}  // namespace neuro
