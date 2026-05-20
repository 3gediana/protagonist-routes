#pragma once

#include "core/interfaces/IAgentCapability.h"

namespace neuro {

// Mating capability: agent can choose to mate with a nearby same-species agent.
// Output[0] > 0.5 triggers mate attempt.
// Requires: both agents alive, same species, within mate_radius, sufficient energy.
// On success: both agents lose energy, the mating event is recorded for
// the evolution system to use spatially-informed parent selection.
class MateCapability final : public IAgentCapability {
public:
    MateCapability(float mate_radius, float energy_cost);

    std::size_t consumedOutputs() const override { return 1; }
    void apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) override;
    const char* name() const override { return "mate"; }

private:
    float mate_radius_;
    float energy_cost_;
};

}  // namespace neuro
