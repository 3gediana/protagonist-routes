#pragma once

#include "core/interfaces/IAgentCapability.h"

namespace neuro {

class TerrainModifier final : public IAgentCapability {
public:
    TerrainModifier(float dig_rate, float build_rate, float energy_cost_per_unit);

    void apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) override;
    std::size_t consumedOutputs() const override { return 2; }
    const char* name() const override { return "terrain_modifier"; }

private:
    float dig_rate_;      // height reduction per second at full output
    float build_rate_;    // height increase per second at full output
    float energy_cost_;   // energy cost per unit of height change
};

}  // namespace neuro
