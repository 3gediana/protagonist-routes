#pragma once

#include "core/interfaces/IAgentCapability.h"

namespace neuro::routes::protagonist {

class GoalActionAssistCapability final : public IAgentCapability {
public:
    GoalActionAssistCapability(float assist_speed = 5.0f, float pick_radius = 5.0f);

    std::size_t consumedOutputs() const override { return 0; }
    void apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) override;
    const char* name() const override { return "goal_action_assist"; }

private:
    float assist_speed_;
    float pick_radius_;
};

}  // namespace neuro::routes::protagonist
