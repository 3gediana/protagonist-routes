// L2.2 homeostatic drive state v2 (SPECIES_DIFFERENTIATION_SPEC.md § 2):
//   - 5 drive states (hunger / thirst / fatigue / fear / loneliness)
//   - DriveTickCapability advances them per-tick from velocity, food
//     eaten, peer / predator presence
//   - per-species DriveSetpoints (apex loneliness 0.6, pack 0.0, etc)
//   - driveSetpointPenalty() reward formula
#include "core/agent/Agent.h"
#include "core/brain/fixed/FixedTopologyBrain.h"
#include "core/brain/fixed/FixedTopologyGenome.h"
#include "core/capability/DriveTickCapability.h"
#include "core/perception/SelfPerception.h"
#include "core/types/Vec2.h"
#include "core/world/World2D.h"
#include "core/world/layers/AgentLayer.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <memory>
#include <random>

namespace {

using neuro::Agent;
using neuro::AgentId;
using neuro::AgentLayer;
using neuro::DriveTickCapability;
using neuro::FixedTopologyBrain;
using neuro::FixedTopologyGenome;
using neuro::GenomeId;
using neuro::SelfPerception;
using neuro::Vec2;
using neuro::World2D;
using neuro::WorldId;

std::unique_ptr<Agent> makeAgent(AgentId id, Vec2 pos) {
    std::mt19937 rng(static_cast<std::uint32_t>(id.value + 31));
    auto genome = FixedTopologyGenome::random(
        GenomeId{id.value}, /*input_dim=*/2, /*hidden_dim=*/2, /*output_dim=*/1, rng);
    auto brain = std::make_unique<FixedTopologyBrain>(*genome);
    auto agent = std::make_unique<Agent>(
        id, GenomeId{static_cast<std::uint64_t>(id.value)},
        std::move(brain), pos, 100.0f);
    agent->addPerception(std::make_unique<SelfPerception>(200.0f, 5000.0));
    return agent;
}

}  // namespace

// ---- Setpoint penalty ----

TEST(DriveStateL22, PenaltyZeroAtSetpoint) {
    auto a = makeAgent(AgentId{1}, Vec2{0, 0});
    Agent::DriveSetpoints sp{};
    sp.loneliness = 0.6f;
    a->setDriveSetpoints(sp);
    Agent::DriveState ds{};
    ds.loneliness = 0.6f;  // exactly at setpoint
    a->setDriveState(ds);
    EXPECT_NEAR(a->driveSetpointPenalty(), 0.0f, 1e-6f);
}

TEST(DriveStateL22, PenaltyNegativeAwayFromSetpoint) {
    auto a = makeAgent(AgentId{1}, Vec2{0, 0});
    Agent::DriveState ds{};
    ds.hunger = 0.5f;
    ds.fear = 0.7f;
    a->setDriveState(ds);
    // setpoint default = 0.0; penalty = -(0.5^2 + 0.7^2) = -(0.25 + 0.49) = -0.74
    EXPECT_NEAR(a->driveSetpointPenalty(), -0.74f, 1e-5f);
}

// ---- DriveTickCapability ----

TEST(DriveStateL22, HungerGrowsOverTimeAndDropsOnFood) {
    auto a = makeAgent(AgentId{1}, Vec2{0, 0});
    World2D world(WorldId{1}, Vec2{60, 60});
    DriveTickCapability cap;

    // 50 ticks of dt=0.1 = 5s passive growth.
    for (int i = 0; i < 50; ++i) cap.apply(*a, world, {}, 0.1f);
    EXPECT_GT(a->driveState().hunger, 0.0f);
    const float h_before_food = a->driveState().hunger;

    // Now feed: noteFoodEaten populates food_eaten_this_tick_, which is
    // consumed by the drive tick this tick (then beginTick would reset).
    a->noteFoodEaten(50.0f);  // a lot of food
    cap.apply(*a, world, {}, 0.1f);
    EXPECT_LT(a->driveState().hunger, h_before_food)
        << "Drive tick should drop hunger when food was eaten this tick";
}

TEST(DriveStateL22, FatigueGrowsWithMovementAndDecaysAtRest) {
    auto a = makeAgent(AgentId{1}, Vec2{0, 0});
    World2D world(WorldId{1}, Vec2{60, 60});
    DriveTickCapability cap;

    // Move agent (set velocity).
    a->setVelocity(Vec2{3.0f, 0.0f});
    for (int i = 0; i < 30; ++i) cap.apply(*a, world, {}, 0.1f);
    EXPECT_GT(a->driveState().fatigue, 0.0f);

    // Now rest.
    a->setVelocity(Vec2{0.0f, 0.0f});
    const float f_before_rest = a->driveState().fatigue;
    for (int i = 0; i < 50; ++i) cap.apply(*a, world, {}, 0.1f);
    EXPECT_LT(a->driveState().fatigue, f_before_rest);
}

TEST(DriveStateL22, FearGrowsWhenPredatorInRadius) {
    AgentLayer* agents = nullptr;
    auto world = std::make_unique<World2D>(WorldId{1}, Vec2{60, 60});
    auto agent_layer = std::make_unique<AgentLayer>(-1.0e9f, 0.0f, 1);
    agents = agent_layer.get();
    world->addLayer(std::move(agent_layer));

    auto self = makeAgent(AgentId{1}, Vec2{30, 30});
    auto* self_raw = self.get();
    agents->addAgent(std::move(self), *world);

    // Predator inside sense_radius (default 24).
    auto pred = makeAgent(AgentId{2}, Vec2{40, 30});
    pred->setPredator(true);
    agents->addAgent(std::move(pred), *world);

    DriveTickCapability cap;
    for (int i = 0; i < 30; ++i) cap.apply(*self_raw, *world, {}, 0.1f);
    EXPECT_GT(self_raw->driveState().fear, 0.0f);
}

TEST(DriveStateL22, FearDecaysWithoutPredator) {
    auto a = makeAgent(AgentId{1}, Vec2{0, 0});
    Agent::DriveState ds{};
    ds.fear = 0.7f;
    a->setDriveState(ds);
    World2D world(WorldId{1}, Vec2{60, 60});
    auto agent_layer = std::make_unique<AgentLayer>(-1.0e9f, 0.0f, 1);
    world.addLayer(std::move(agent_layer));

    DriveTickCapability cap;
    for (int i = 0; i < 30; ++i) cap.apply(*a, world, {}, 0.1f);
    EXPECT_LT(a->driveState().fear, 0.7f);
}

TEST(DriveStateL22, LonelinessDropsWithPeerInRadius) {
    AgentLayer* agents = nullptr;
    auto world = std::make_unique<World2D>(WorldId{1}, Vec2{60, 60});
    auto agent_layer = std::make_unique<AgentLayer>(-1.0e9f, 0.0f, 1);
    agents = agent_layer.get();
    world->addLayer(std::move(agent_layer));

    auto self = makeAgent(AgentId{1}, Vec2{30, 30});
    auto* self_raw = self.get();
    Agent::DriveState ds{};
    ds.loneliness = 0.5f;  // start mid-lonely
    self_raw->setDriveState(ds);
    agents->addAgent(std::move(self), *world);

    // Peer in radius (non-predator).
    auto peer = makeAgent(AgentId{2}, Vec2{32, 30});
    agents->addAgent(std::move(peer), *world);

    DriveTickCapability cap;
    for (int i = 0; i < 30; ++i) cap.apply(*self_raw, *world, {}, 0.1f);
    EXPECT_LT(self_raw->driveState().loneliness, 0.5f);
}

TEST(DriveStateL22, LonelinessGrowsAlone) {
    AgentLayer* agents = nullptr;
    auto world = std::make_unique<World2D>(WorldId{1}, Vec2{60, 60});
    auto agent_layer = std::make_unique<AgentLayer>(-1.0e9f, 0.0f, 1);
    agents = agent_layer.get();
    world->addLayer(std::move(agent_layer));

    auto self = makeAgent(AgentId{1}, Vec2{30, 30});
    auto* self_raw = self.get();
    agents->addAgent(std::move(self), *world);

    DriveTickCapability cap;
    for (int i = 0; i < 30; ++i) cap.apply(*self_raw, *world, {}, 0.1f);
    EXPECT_GT(self_raw->driveState().loneliness, 0.0f);
}

TEST(DriveStateL22, DrivesClampedToOne) {
    auto a = makeAgent(AgentId{1}, Vec2{0, 0});
    World2D world(WorldId{1}, Vec2{60, 60});
    DriveTickCapability cap;
    a->setVelocity(Vec2{20.0f, 0.0f});  // very fast = lots of fatigue
    for (int i = 0; i < 5000; ++i) cap.apply(*a, world, {}, 0.1f);
    EXPECT_LE(a->driveState().fatigue, 1.0f);
    EXPECT_LE(a->driveState().hunger, 1.0f);
}
