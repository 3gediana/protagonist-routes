#include <gtest/gtest.h>

#include "core/agent/Agent.h"
#include "core/brain/fixed/FixedTopologyBrain.h"
#include "core/brain/fixed/FixedTopologyGenome.h"

#include <random>

namespace {

std::unique_ptr<neuro::IBrain> makeBrain() {
    std::mt19937 rng(42);
    auto genome = neuro::FixedTopologyGenome::random(neuro::GenomeId{1}, 5, 8, 2, rng);
    return std::make_unique<neuro::FixedTopologyBrain>(*genome);
}

}  // namespace

TEST(Agent, DecisionIntervalDefaultsToOne) {
    auto brain = makeBrain();
    neuro::Agent agent(neuro::AgentId{1}, neuro::GenomeId{1}, std::move(brain), neuro::Vec2{0, 0}, 100.0f, 20, 1.0f);
    EXPECT_FLOAT_EQ(agent.decisionInterval(), 1.0f);
}

TEST(Agent, DecisionIntervalCustom) {
    auto brain = makeBrain();
    neuro::Agent agent(neuro::AgentId{1}, neuro::GenomeId{1}, std::move(brain), neuro::Vec2{0, 0}, 100.0f, 20, 5.0f);
    EXPECT_FLOAT_EQ(agent.decisionInterval(), 5.0f);
}

TEST(Agent, ShouldDecideInitially) {
    auto brain = makeBrain();
    neuro::Agent agent(neuro::AgentId{1}, neuro::GenomeId{1}, std::move(brain), neuro::Vec2{0, 0}, 100.0f, 20, 5.0f);
    EXPECT_TRUE(agent.shouldDecide(0));
}

TEST(Agent, ShouldNotDecideBeforeInterval) {
    auto brain = makeBrain();
    neuro::Agent agent(neuro::AgentId{1}, neuro::GenomeId{1}, std::move(brain), neuro::Vec2{0, 0}, 100.0f, 20, 5.0f);
    agent.setNextDecisionTick(5);
    EXPECT_FALSE(agent.shouldDecide(3));
    EXPECT_TRUE(agent.shouldDecide(5));
}
