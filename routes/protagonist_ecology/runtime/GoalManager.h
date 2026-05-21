#pragma once

#include "brain/GoalMemory.h"

#include <cstddef>
#include <cstdint>

namespace neuro::routes::protagonist {

struct GoalDecisionContext {
    GoalMemoryState goal_state{};
    float health_ratio{1.0f};
    float hydration_ratio{1.0f};
    float energy{100.0f};
    bool predator_nearby{false};
    bool predator_warn_enabled{true};
    std::size_t alive_protagonists{1};
    bool peer_signal_nearby{false};
    bool fire_nearby{false};
    bool worksite_nearby{false};
    bool remembered_worksite{false};
    bool carrying{false};
    bool site_needs_sticks{false};
    bool site_needs_stones{false};
};

enum class GoalDecisionAction : std::uint8_t {
    KeepCurrent = 0,
    SetGoal = 1,
};

struct GoalDecision {
    GoalDecisionAction action{GoalDecisionAction::KeepCurrent};
    GoalType goal{GoalType::Explore};
    float commitment{0.0f};
};

bool shouldRefreshRuleBasedGoal(const GoalMemoryState& state);
GoalDecision decideRuleBasedGoal(const GoalDecisionContext& context);

}  // namespace neuro::routes::protagonist
