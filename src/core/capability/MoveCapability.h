#pragma once

#include "core/interfaces/IAgentCapability.h"

namespace neuro {

class MoveCapability final : public IAgentCapability {
public:
    MoveCapability(float max_speed,
                   float decision_cost,
                   float movement_cost_factor,
                   std::size_t exploration_grid_size = 20,
                   bool aerial = false);

    std::size_t consumedOutputs() const override;
    void apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) override;
    const char* name() const override;

private:
    float max_speed_;
    float decision_cost_;
    float movement_cost_factor_;
    std::size_t exploration_grid_size_;
    bool aerial_;  // aerial agents bypass terrain slope/cave penalties
};

}  // namespace neuro
