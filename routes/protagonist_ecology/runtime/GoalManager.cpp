#include "runtime/GoalManager.h"

namespace neuro::routes::protagonist {

namespace {

constexpr float kGoalRefreshSeconds = 6.0f;
constexpr float kLowCommitmentThreshold = 0.20f;
constexpr float kCriticalHealthRatio = 0.30f;
constexpr float kLowHydrationRatio = 0.40f;
constexpr float kSatisfiedHydrationRatio = 0.90f;
constexpr float kRestHealthRatio = 0.70f;
constexpr float kLowEnergyThreshold = 70.0f;

GoalDecision keepCurrent() {
    return {};
}

GoalDecision setGoal(GoalType goal, float commitment) {
    GoalDecision out;
    out.action = GoalDecisionAction::SetGoal;
    out.goal = goal;
    out.commitment = commitment;
    return out;
}

}  // namespace

bool shouldRefreshRuleBasedGoal(const GoalMemoryState& state) {
    return state.elapsed_seconds >= kGoalRefreshSeconds
        || state.commitment <= kLowCommitmentThreshold;
}

GoalDecision decideRuleBasedGoal(const GoalDecisionContext& context) {
    const bool should_refresh_goal = shouldRefreshRuleBasedGoal(context.goal_state);
    const auto set_if_new = [&](GoalType goal, float commitment) {
        if (should_refresh_goal || context.goal_state.current_goal != goal) {
            return setGoal(goal, commitment);
        }
        return keepCurrent();
    };

    // Flee is highest-priority and overrides commitment.
    if (context.health_ratio <= kCriticalHealthRatio) {
        return setGoal(GoalType::Flee, 1.0f);
    }
    // Predator seen but health OK -> warn the group.
    if (context.predator_nearby && context.predator_warn_enabled && context.alive_protagonists >= 2) {
        return set_if_new(GoalType::EmitSignal, 0.80f);
    }
    if (context.predator_nearby) {
        return setGoal(GoalType::Flee, 1.0f);
    }

    if (context.goal_state.current_goal == GoalType::FindWater
        && context.goal_state.commitment > kLowCommitmentThreshold
        && context.hydration_ratio < kSatisfiedHydrationRatio) {
        return keepCurrent();
    }
    if (context.hydration_ratio <= kLowHydrationRatio) {
        return set_if_new(GoalType::FindWater, 0.95f);
    }

    if (context.goal_state.current_goal == GoalType::BuildWorksite
        && context.goal_state.commitment > kLowCommitmentThreshold
        && context.carrying
        && context.remembered_worksite) {
        return keepCurrent();
    }
    if (context.carrying && context.remembered_worksite) {
        return set_if_new(GoalType::BuildWorksite, 0.85f);
    }
    if (context.site_needs_sticks && !context.carrying) {
        return set_if_new(GoalType::GatherStick, 0.70f);
    }
    if (context.site_needs_stones && !context.carrying) {
        return set_if_new(GoalType::GatherStone, 0.70f);
    }

    // RespondSignal so listeners actually act on heard signals.
    if (context.peer_signal_nearby) {
        return set_if_new(GoalType::RespondSignal, 0.60f);
    }
    if (context.fire_nearby && context.health_ratio <= kRestHealthRatio) {
        return set_if_new(GoalType::RestNearFire, 0.70f);
    }
    if (context.energy <= kLowEnergyThreshold) {
        return set_if_new(GoalType::FindFood, 0.75f);
    }
    if (should_refresh_goal) {
        return setGoal(GoalType::Explore, 0.55f);
    }

    return keepCurrent();
}

}  // namespace neuro::routes::protagonist
