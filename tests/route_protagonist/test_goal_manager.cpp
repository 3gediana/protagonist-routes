#include "runtime/GoalManager.h"

#include <gtest/gtest.h>

using neuro::routes::protagonist::GoalDecisionAction;
using neuro::routes::protagonist::GoalDecisionContext;
using neuro::routes::protagonist::GoalType;
using neuro::routes::protagonist::decideRuleBasedGoal;

TEST(GoalManager, PredatorWithGroupChoosesEmitSignalBeforeFlee) {
    GoalDecisionContext context;
    context.goal_state.current_goal = GoalType::Explore;
    context.goal_state.commitment = 0.9f;
    context.health_ratio = 0.85f;
    context.predator_nearby = true;
    context.predator_warn_enabled = true;
    context.alive_protagonists = 2;

    const auto decision = decideRuleBasedGoal(context);

    EXPECT_EQ(decision.action, GoalDecisionAction::SetGoal);
    EXPECT_EQ(decision.goal, GoalType::EmitSignal);
    EXPECT_FLOAT_EQ(decision.commitment, 0.80f);
}

TEST(GoalManager, PredatorWithGroupCanPreferFleeWhenWarnDisabled) {
    GoalDecisionContext context;
    context.goal_state.current_goal = GoalType::Explore;
    context.goal_state.commitment = 0.9f;
    context.health_ratio = 0.85f;
    context.predator_nearby = true;
    context.predator_warn_enabled = false;
    context.alive_protagonists = 2;

    const auto decision = decideRuleBasedGoal(context);

    EXPECT_EQ(decision.action, GoalDecisionAction::SetGoal);
    EXPECT_EQ(decision.goal, GoalType::Flee);
    EXPECT_FLOAT_EQ(decision.commitment, 1.0f);
}

TEST(GoalManager, PeerSignalRespondsWithoutRefreshGate) {
    GoalDecisionContext context;
    context.goal_state.current_goal = GoalType::FindFood;
    context.goal_state.commitment = 0.9f;
    context.goal_state.elapsed_seconds = 1.0f;
    context.health_ratio = 0.9f;
    context.hydration_ratio = 0.95f;
    context.energy = 95.0f;
    context.peer_signal_nearby = true;

    const auto decision = decideRuleBasedGoal(context);

    EXPECT_EQ(decision.action, GoalDecisionAction::SetGoal);
    EXPECT_EQ(decision.goal, GoalType::RespondSignal);
    EXPECT_FLOAT_EQ(decision.commitment, 0.60f);
}

TEST(GoalManager, FindWaterStaysStickyUntilRecovered) {
    GoalDecisionContext context;
    context.goal_state.current_goal = GoalType::FindWater;
    context.goal_state.commitment = 0.8f;
    context.goal_state.elapsed_seconds = 7.0f;
    context.health_ratio = 0.95f;
    context.hydration_ratio = 0.55f;
    context.energy = 95.0f;

    const auto decision = decideRuleBasedGoal(context);

    EXPECT_EQ(decision.action, GoalDecisionAction::KeepCurrent);
}

TEST(GoalManager, CarryingMaterialReturnsToRememberedWorksite) {
    GoalDecisionContext context;
    context.goal_state.current_goal = GoalType::GatherStick;
    context.goal_state.commitment = 0.8f;
    context.goal_state.elapsed_seconds = 1.0f;
    context.health_ratio = 0.95f;
    context.hydration_ratio = 0.95f;
    context.energy = 95.0f;
    context.remembered_worksite = true;
    context.carrying = true;

    const auto decision = decideRuleBasedGoal(context);

    EXPECT_EQ(decision.action, GoalDecisionAction::SetGoal);
    EXPECT_EQ(decision.goal, GoalType::BuildWorksite);
    EXPECT_FLOAT_EQ(decision.commitment, 0.85f);
}
