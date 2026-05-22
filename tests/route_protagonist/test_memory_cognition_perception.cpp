#include "brain/ProtagonistBrain.h"
#include "core/agent/Agent.h"
#include "core/world/World2D.h"
#include "perception/MemoryCognitionPerception.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <memory>

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

// Audit § 4.1 cascade decoupling: dim 32-35 are now goal-only (kind,
// commitment, progress, elapsed). Spatial direction still lives on
// dim 0-17 but is no longer fused into the goal block. The three
// historical "GoalTargetUses..." tests are rewritten to lock in this
// new contract so a future cascade reintroduction would fail loudly.

TEST(MemoryCognitionPerception, GoalBlockIsPureGoalMemoryNotSpatialCascade) {
    // Spatial memory has water but dim 34-35 should NOT mirror that.
    // Instead dim 34 must reflect goal.completionProgress() and
    // dim 35 must reflect goal.elapsedSeconds()/60 - both intrinsic
    // to GoalMemory.
    World2D world(WorldId{1}, Vec2{120.0f, 120.0f});
    auto fixture = makeStubProtagonist(AgentId{1}, Vec2{50.0f, 50.0f});
    fixture.brain->spatialMemory().recordSighting(Vec2{80.0f, 50.0f}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    fixture.brain->goalMemory().setGoal(GoalType::FindWater, 0.8f);
    fixture.brain->goalMemory().progress(0.4f);
    fixture.brain->goalMemory().tick(12.0f);  // elapsed_seconds = 12

    MemoryCognitionPerception perception;
    std::array<float, MemoryCognitionPerception::kDim> output{};
    perception.sense(*fixture.agent, world, output);

    // Goal kind / commitment after tick(12) which decays commitment by
    // 0.015*12 = 0.18 from 0.8 to 0.62.
    EXPECT_NEAR(output[32], 1.0f / static_cast<float>(GoalType::Count), 1.0e-5f);
    EXPECT_NEAR(output[33], 0.62f, 1.0e-4f);
    // dim 34 = completionProgress() = 0.4 (audit § 4.1 new semantics)
    EXPECT_NEAR(output[34], 0.4f, 1.0e-5f);
    // dim 35 = elapsedSeconds / 60 = 12 / 60 = 0.2 (audit § 4.1 new semantics)
    EXPECT_NEAR(output[35], 0.2f, 1.0e-5f);
    // Spatial water dxdy still on dim 0-1 (unchanged path).
    EXPECT_NEAR(output[0], 0.15f, 0.02f);
    EXPECT_NEAR(output[1], 0.0f, 0.02f);
}

TEST(MemoryCognitionPerception, GoalBlockZeroWhenGoalMemoryUntouched) {
    // Default goal state (Explore, commitment=1.0, progress=0, elapsed=0):
    // dim 32 = 0/Count = 0; dim 33 = 1.0; dim 34 = 0; dim 35 = 0.
    // The ally sighting on social memory should leak only into dim 24-27,
    // not into the goal block.
    World2D world(WorldId{2}, Vec2{120.0f, 120.0f});
    auto fixture = makeStubProtagonist(AgentId{1}, Vec2{50.0f, 50.0f});
    fixture.brain->socialMemory().recordSighting(AgentId{9}, Vec2{50.0f, 80.0f}, 0, 0.0, false);

    MemoryCognitionPerception perception;
    std::array<float, MemoryCognitionPerception::kDim> output{};
    perception.sense(*fixture.agent, world, output);

    EXPECT_FLOAT_EQ(output[32], 0.0f);
    EXPECT_FLOAT_EQ(output[33], 1.0f);
    EXPECT_FLOAT_EQ(output[34], 0.0f);
    EXPECT_FLOAT_EQ(output[35], 0.0f);
    // Ally still on the social block (dim 24-25 = dxdy to ally at (50,80))
    EXPECT_NEAR(output[24], 0.0f, 1.0e-5f);
    EXPECT_NEAR(output[25], 0.15f, 1.0e-5f);
}

// Audit § 4.2 / § 4.4 fix: dim 18-21 now specialise to peer_signal_heard,
// the only episodic event whose pos / salience / recency the controller
// needs to walk back to a signal source.

TEST(MemoryCognitionPerception, PeerSignalHeardReadbackOnDim18To21) {
    World2D world(WorldId{3}, Vec2{120.0f, 120.0f});
    auto fixture = makeStubProtagonist(AgentId{1}, Vec2{50.0f, 50.0f});
    // Write a peer_signal_heard event 6 seconds ago at (80, 50).
    // perception dim 18-19 should resolve to ((80-50)/200, (50-50)/200)
    //                                       = (0.15, 0.0)
    // dim 20 should be the salience (0.7) clamped to [0,1].
    // dim 21 should be 1 - 6/60 = 0.9.
    constexpr neuro::SimTimeSeconds kEventTime = 4.0;  // event happened at t=4
    fixture.brain->episodicMemory().recordEvent(
        neuro::Tick{0},
        kEventTime,
        Vec2{80.0f, 50.0f},
        "peer_signal_heard",
        0.7f);
    // World currentTimeSeconds() defaults to 0; we need to advance it.
    world.tick(10.0f);  // now_time = 10s, event age = 6s

    MemoryCognitionPerception perception;
    std::array<float, MemoryCognitionPerception::kDim> output{};
    perception.sense(*fixture.agent, world, output);

    EXPECT_NEAR(output[18], 0.15f, 1.0e-5f);
    EXPECT_NEAR(output[19], 0.0f, 1.0e-5f);
    EXPECT_NEAR(output[20], 0.7f, 1.0e-5f);
    EXPECT_NEAR(output[21], 0.9f, 1.0e-5f);
}

TEST(MemoryCognitionPerception, PeerSignalHeardBlockIsZeroWithoutEvent) {
    // Episodic memory contains a high-salience built_worksite event but
    // no peer_signal_heard. Old wiring exposed topSalient(1), which
    // would have lit up dim 18-21 with the build event - that misleads
    // the controller. New wiring keeps dim 18-21 silent unless there
    // is a real peer signal to walk toward.
    World2D world(WorldId{4}, Vec2{120.0f, 120.0f});
    auto fixture = makeStubProtagonist(AgentId{1}, Vec2{50.0f, 50.0f});
    fixture.brain->episodicMemory().recordEvent(
        neuro::Tick{0},
        0.0,
        Vec2{80.0f, 50.0f},
        "built_worksite",
        0.95f);

    MemoryCognitionPerception perception;
    std::array<float, MemoryCognitionPerception::kDim> output{};
    perception.sense(*fixture.agent, world, output);

    EXPECT_FLOAT_EQ(output[18], 0.0f);
    EXPECT_FLOAT_EQ(output[19], 0.0f);
    EXPECT_FLOAT_EQ(output[20], 0.0f);
    EXPECT_FLOAT_EQ(output[21], 0.0f);
}
