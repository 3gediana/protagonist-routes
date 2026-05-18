#pragma once

#include "core/interfaces/IAgentCapability.h"

#include <cstddef>

namespace neuro {

class EmitSignalCapability final : public IAgentCapability {
public:
    EmitSignalCapability(std::size_t channels, float energy_cost);

    std::size_t consumedOutputs() const override;
    void apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) override;
    const char* name() const override;

    std::size_t channels() const { return channels_; }

private:
    std::size_t channels_;
    float energy_cost_;
};

}  // namespace neuro
