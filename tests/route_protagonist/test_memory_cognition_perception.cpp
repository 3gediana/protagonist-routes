#include "brain/ProtagonistBrain.h"
#include "core/agent/Agent.h"
#include "core/world/World2D.h"
#include "core/world/layers/signal/SignalLayer.h"
#include "perception/MemoryCognitionPerception.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace {

using neuro::AgentId;
using neuro::GenomeId;
using neuro::Vec2;
using neuro::World2D;
using neuro::WorldId;
using neuro::routes::protagonist::GoalType;
using neuro::routes::protagonist::MemoryCognitionPerception;
using neuro::routes::protagonist::ProtagonistBrain;

struct ProtagonistFixture {
    std::unique_ptr<neuro::Agent> agent;
    ProtagonistBrain* brain{nullptr};
};

ProtagonistFixture makeStubProtagonist(AgentId id, Vec2 pos) {
    auto brain = std::make_unique<ProtagonistBrain>(
        MemoryCognitionPerception::kDim,
        8,
        4,
        8,
        2,
        3,
        static_cast<std::uint32_t>(id.value + 17),
        false);
    auto* brain_ptr = brain.get();
    ProtagonistFixture out;
    out.agent = std::make_unique<neuro::Agent>(
        id,
        GenomeId{static_cast<std::uint64_t>(id.value)},
        std::move(brain),
        pos,
        1000.0f);
    out.brain = brain_ptr;
    return out;
}

}  // namespace

TEST(MemoryCognitionPerception, GoalTargetUsesWaterMemoryForFindWater) {
    World2D world(WorldId{1}, Vec2{120.0f, 120.0f});
    auto fixture = makeStubProtagonist(AgentId{1}, Vec2{50.0f, 50.0f});
    fixture.brain->spatialMemory().recordSighting(Vec2{80.0f, 50.0f}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    fixture.brain->goalMemory().setGoal(GoalType::FindWater, 0.8f);

    MemoryCognitionPerception perception;
    std::array<float, MemoryCognitionPerception::kDim> output{};
    perception.sense(*fixture.agent, world, output);

    EXPECT_NEAR(output[32], 1.0f / static_cast<float>(GoalType::Count), 1.0e-5f);
    EXPECT_FLOAT_EQ(output[33], 0.8f);
    EXPECT_NEAR(output[34], 0.15f, 0.02f);
    EXPECT_NEAR(output[35], 0.0f, 0.02f);
}

TEST(MemoryCognitionPerception, GoalTargetUsesAllyLocationForRespondSignal) {
    World2D world(WorldId{2}, Vec2{120.0f, 120.0f});
    auto fixture = makeStubProtagonist(AgentId{1}, Vec2{50.0f, 50.0f});
    fixture.brain->socialMemory().recordSighting(AgentId{9}, Vec2{50.0f, 80.0f}, 0, 0.0, false);
    fixture.brain->goalMemory().setGoal(GoalType::RespondSignal, 0.6f);

    MemoryCognitionPerception perception;
    std::array<float, MemoryCognitionPerception::kDim> output{};
    perception.sense(*fixture.agent, world, output);

    EXPECT_NEAR(output[32],
                static_cast<float>(GoalType::RespondSignal) / static_cast<float>(GoalType::Count),
                1.0e-5f);
    EXPECT_FLOAT_EQ(output[33], 0.6f);
    EXPECT_NEAR(output[34], 0.0f, 1.0e-5f);
    EXPECT_NEAR(output[35], 0.15f, 1.0e-5f);
}

TEST(MemoryCognitionPerception, GoalTargetUsesRecentSignalSourceForRespondSignal) {
    World2D world(WorldId{3}, Vec2{120.0f, 120.0f});
    world.addLayer(std::make_unique<neuro::SignalLayer>(Vec2{120.0f, 120.0f}, 32, 2, 0.05f, 0.08f));

    auto fixture = makeStubProtagonist(AgentId{1}, Vec2{50.0f, 50.0f});
    fixture.brain->socialMemory().recordSighting(AgentId{9}, Vec2{50.0f, 80.0f}, 0, 0.0, false);
    fixture.brain->goalMemory().setGoal(GoalType::RespondSignal, 0.6f);

    auto* signal_layer = world.getLayer<neuro::SignalLayer>();
    ASSERT_NE(signal_layer, nullptr);
    signal_layer->emitWithHistory(
        AgentId{0},
        Vec2{68.0f, 50.0f},
        std::vector<float>{1.0f, 0.0f},
        world.currentTick(),
        world.currentTimeSeconds());

    MemoryCognitionPerception perception;
    std::array<float, MemoryCognitionPerception::kDim> output{};
    perception.sense(*fixture.agent, world, output);

    EXPECT_NEAR(output[32],
                static_cast<float>(GoalType::RespondSignal) / static_cast<float>(GoalType::Count),
                1.0e-5f);
    EXPECT_FLOAT_EQ(output[33], 0.6f);
    EXPECT_NEAR(output[34], 0.09f, 1.0e-5f);
    EXPECT_NEAR(output[35], 0.0f, 1.0e-5f);
}
