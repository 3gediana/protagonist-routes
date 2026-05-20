#include "core/agent/Agent.h"
#include "core/brain/fixed/FixedTopologyBrain.h"
#include "core/brain/fixed/FixedTopologyGenome.h"
#include "core/reward/FoodReward.h"
#include "core/reward/IntrinsicReward.h"
#include "core/reward/SurvivalReward.h"
#include "core/world/World2D.h"

#include <gtest/gtest.h>

#include <cmath>

using neuro::Agent;
using neuro::FixedTopologyBrain;
using neuro::FixedTopologyGenome;
using neuro::FoodReward;
using neuro::GenomeId;
using neuro::IntrinsicReward;
using neuro::SurvivalReward;
using neuro::World2D;
using neuro::WorldId;

TEST(Rewards, SurvivalRewardReturnsBaseTickBonus) {
    FixedTopologyGenome genome(GenomeId{1}, 5, 10, 2);
    auto brain = std::make_unique<FixedTopologyBrain>(genome);
    Agent agent(neuro::AgentId{1}, GenomeId{1}, std::move(brain), {0.0f, 0.0f}, 10.0f);
    World2D world(WorldId{1}, {10.0f, 10.0f});

    SurvivalReward reward(1.0);
    EXPECT_DOUBLE_EQ(reward.evaluate(agent, world, 1.0f), 0.01);
}

TEST(Rewards, FoodRewardReadsAgentTickState) {
    FixedTopologyGenome genome(GenomeId{1}, 5, 10, 2);
    auto brain = std::make_unique<FixedTopologyBrain>(genome);
    Agent agent(neuro::AgentId{1}, GenomeId{1}, std::move(brain), {0.0f, 0.0f}, 10.0f);
    World2D world(WorldId{1}, {10.0f, 10.0f});

    agent.beginTick();
    agent.noteFoodEaten(3.5f);
    agent.noteFoodEaten(8.0f);
    FoodReward reward(10.0);
    EXPECT_DOUBLE_EQ(reward.evaluate(agent, world, 1.0f), 11.5);
    EXPECT_DOUBLE_EQ(reward.weight(), 10.0);
}

TEST(Rewards, IntrinsicRewardDetectsNewCell) {
    FixedTopologyGenome genome(GenomeId{1}, 5, 10, 2);
    auto brain = std::make_unique<FixedTopologyBrain>(genome);
    Agent agent(neuro::AgentId{1}, GenomeId{1}, std::move(brain), {0.0f, 0.0f}, 10.0f);
    World2D world(WorldId{1}, {10.0f, 10.0f});

    agent.beginTick();
    agent.noteExplorationCell(7);
    IntrinsicReward reward(5.0);
    EXPECT_DOUBLE_EQ(reward.evaluate(agent, world, 1.0f), 1.0);

    agent.beginTick();
    agent.noteExplorationCell(9);
    EXPECT_NEAR(reward.evaluate(agent, world, 1.0f), 1.0 / std::sqrt(2.0), 1e-9);

    EXPECT_DOUBLE_EQ(reward.weight(), 5.0);
}
