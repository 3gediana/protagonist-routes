#include "brain/GoalMemory.h"
#include "brain/ProtagonistBrain.h"
#include "capability/GoalActionAssistCapability.h"
#include "capability/GoalAwareDropCapability.h"
#include "core/agent/Agent.h"
#include "core/types/Vec2.h"
#include "core/world/World2D.h"
#include "core/world/layers/AgentLayer.h"
#include "core/world/layers/object/ObjectLayer.h"
#include "world/BaseLayer.h"
#include "world/SurvivalStatusLayer.h"
#include "world/WaterLayer.h"
#include "world/WorksiteLayer.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <span>

namespace {

using neuro::AgentId;
using neuro::GenomeId;
using neuro::MovableObject;
using neuro::ObjectType;
using neuro::Vec2;
using neuro::World2D;
using neuro::WorldId;
using neuro::kZeroVec2;
using neuro::routes::protagonist::BaseLayer;
using neuro::routes::protagonist::GoalActionAssistCapability;
using neuro::routes::protagonist::GoalAwareDropCapability;
using neuro::routes::protagonist::GoalType;
using neuro::routes::protagonist::ProtagonistBrain;
using neuro::routes::protagonist::SurvivalStatusLayer;
using neuro::routes::protagonist::WaterLayer;
using neuro::routes::protagonist::WorksiteLayer;

struct ProtagonistFixture {
    std::unique_ptr<neuro::Agent> agent;
    neuro::Agent* raw_agent{nullptr};
    ProtagonistBrain* brain{nullptr};
};

ProtagonistFixture makeStubProtagonist(AgentId id, Vec2 pos) {
    auto brain = std::make_unique<ProtagonistBrain>(
        8,
        8,
        4,
        8,
        2,
        3,
        static_cast<std::uint32_t>(id.value + 17),
        false);
    auto* brain_ptr = brain.get();
    auto agent = std::make_unique<neuro::Agent>(
        id,
        GenomeId{static_cast<std::uint64_t>(id.value)},
        std::move(brain),
        pos,
        1000.0f);
    auto* agent_ptr = agent.get();
    return ProtagonistFixture{
        .agent = std::move(agent),
        .raw_agent = agent_ptr,
        .brain = brain_ptr,
    };
}

}  // namespace

TEST(GoalActionAssist, FindsAndDrinksAtRememberedWater) {
    World2D world(WorldId{1}, Vec2{120.0f, 120.0f});
    world.addLayer(std::make_unique<BaseLayer>(
        Vec2{60.0f, 60.0f}, 18.0f, 8.0f, 10.0f, 14.0f, -8.0f));
    auto agent_layer = std::make_unique<neuro::AgentLayer>(-1.0e9f, 0.0f, 1);
    auto* agents = agent_layer.get();
    world.addLayer(std::move(agent_layer));
    world.addLayer(std::make_unique<WaterLayer>(1, 20.0f, 20.0f, 5.0f, 3.0f, 24.0f, 123u));
    auto survival_layer = std::make_unique<SurvivalStatusLayer>(
        100.0f, 100.0f, 1.0f, 0.25f, 0.0f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.45f);
    auto* survival = survival_layer.get();
    world.addLayer(std::move(survival_layer));

    auto fixture = makeStubProtagonist(AgentId{1}, Vec2{60.0f, 60.0f});
    agents->addAgent(std::move(fixture.agent), world);
    survival->tick(world, 0.0f);

    auto* water = world.getLayer<WaterLayer>();
    ASSERT_NE(water, nullptr);
    ASSERT_FALSE(water->sources().empty());

    fixture.brain->spatialMemory().recordSighting(
        water->sources().front().position, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    fixture.brain->goalMemory().setGoal(GoalType::FindWater, 0.95f);

    GoalActionAssistCapability assist;
    const float before = survival->hydrationRatio(fixture.raw_agent->id());

    for (int step = 0; step < 8 && !water->inDrinkZone(fixture.raw_agent->position()); ++step) {
        assist.apply(*fixture.raw_agent, world, std::span<const float>{}, 1.0f);
    }
    assist.apply(*fixture.raw_agent, world, std::span<const float>{}, 1.0f);

    EXPECT_TRUE(water->inDrinkZone(fixture.raw_agent->position()));
    EXPECT_GT(survival->hydrationRatio(fixture.raw_agent->id()), before);
}

TEST(GoalActionAssist, PicksUpStickAndReturnsItToRememberedWorksite) {
    World2D world(WorldId{2}, Vec2{50.0f, 50.0f});
    world.addLayer(std::make_unique<BaseLayer>(
        Vec2{15.0f, 25.0f}, 6.0f, 4.0f, 3.0f, 12.0f, -8.0f));
    auto agent_layer = std::make_unique<neuro::AgentLayer>(-1.0e9f, 0.0f, 1);
    auto* agents = agent_layer.get();
    world.addLayer(std::move(agent_layer));
    auto object_layer = std::make_unique<neuro::ObjectLayer>(Vec2{50.0f, 50.0f}, 0, 0.9f);
    auto* objects = object_layer.get();
    world.addLayer(std::move(object_layer));
    auto worksite_layer = std::make_unique<WorksiteLayer>(1, 6.0f, 5.0f, 1, 0, 0.10f, 0.0f, 1u);
    auto* worksites = worksite_layer.get();
    world.addLayer(std::move(worksite_layer));

    ASSERT_TRUE(worksites->activateNextDormant());
    const Vec2 site_pos = worksites->sites().front().position;

    objects->objects().push_back(MovableObject{
        objects->nextId(),
        Vec2{20.0f, 25.0f},
        kZeroVec2,
        1.0f,
        ObjectType::Stick,
        false,
        false,
        1u,
    });

    auto fixture = makeStubProtagonist(AgentId{1}, Vec2{12.0f, 25.0f});
    agents->addAgent(std::move(fixture.agent), world);

    fixture.brain->spatialMemory().recordSighting(
        Vec2{20.0f, 25.0f}, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    fixture.brain->spatialMemory().recordSighting(
        site_pos, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

    GoalActionAssistCapability assist;

    fixture.brain->goalMemory().setGoal(GoalType::GatherStick, 0.70f);
    for (int step = 0; step < 8 && !fixture.raw_agent->isCarrying(); ++step) {
        assist.apply(*fixture.raw_agent, world, std::span<const float>{}, 0.5f);
        worksites->tick(world, 0.5f);
    }

    ASSERT_TRUE(fixture.raw_agent->isCarrying());

    fixture.brain->goalMemory().setGoal(GoalType::BuildWorksite, 0.85f);
    for (int step = 0; step < 12 && worksites->sites().front().stored_sticks == 0; ++step) {
        assist.apply(*fixture.raw_agent, world, std::span<const float>{}, 0.5f);
        worksites->tick(world, 0.5f);
    }

    EXPECT_EQ(worksites->sites().front().stored_sticks, 1u);
    EXPECT_FALSE(fixture.raw_agent->isCarrying());
}

TEST(GoalActionAssist, CarryingGatherMaterialFallsBackToRememberedBase) {
    World2D world(WorldId{3}, Vec2{80.0f, 80.0f});
    world.addLayer(std::make_unique<BaseLayer>(
        Vec2{30.0f, 40.0f}, 8.0f, 5.0f, 4.0f, 10.0f, -8.0f));
    auto agent_layer = std::make_unique<neuro::AgentLayer>(-1.0e9f, 0.0f, 1);
    auto* agents = agent_layer.get();
    world.addLayer(std::move(agent_layer));

    auto fixture = makeStubProtagonist(AgentId{1}, Vec2{12.0f, 40.0f});
    fixture.raw_agent->setCarriedObject(99);
    agents->addAgent(std::move(fixture.agent), world);

    fixture.brain->spatialMemory().recordSighting(
        Vec2{30.0f, 40.0f}, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
    fixture.brain->goalMemory().setGoal(GoalType::GatherStick, 0.70f);

    GoalActionAssistCapability assist;
    const float before = Vec2::distanceSquared(fixture.raw_agent->position(), Vec2{30.0f, 40.0f});
    assist.apply(*fixture.raw_agent, world, std::span<const float>{}, 1.0f);
    const float after = Vec2::distanceSquared(fixture.raw_agent->position(), Vec2{30.0f, 40.0f});

    EXPECT_LT(after, before);
}

TEST(GoalAwareDropCapability, HoldsLogisticsMaterialUntilGoalChanges) {
    World2D world(WorldId{4}, Vec2{40.0f, 40.0f});
    auto object_layer = std::make_unique<neuro::ObjectLayer>(Vec2{40.0f, 40.0f}, 0, 0.9f);
    auto* objects = object_layer.get();
    world.addLayer(std::move(object_layer));

    auto fixture = makeStubProtagonist(AgentId{1}, Vec2{20.0f, 20.0f});
    objects->objects().push_back(MovableObject{
        objects->nextId(),
        fixture.raw_agent->position(),
        kZeroVec2,
        1.0f,
        ObjectType::Stick,
        true,
        false,
        1u,
    });
    fixture.raw_agent->setCarriedObject(objects->objects().back().id);

    GoalAwareDropCapability drop(0.004f);
    const float output = 1.0f;

    fixture.brain->goalMemory().setGoal(GoalType::GatherStick, 0.70f);
    drop.apply(*fixture.raw_agent, world, std::span<const float>(&output, 1), 1.0f);
    EXPECT_TRUE(fixture.raw_agent->isCarrying());
    EXPECT_TRUE(objects->objects().back().carried);

    fixture.brain->goalMemory().setGoal(GoalType::Explore, 0.55f);
    drop.apply(*fixture.raw_agent, world, std::span<const float>(&output, 1), 1.0f);
    EXPECT_FALSE(fixture.raw_agent->isCarrying());
    EXPECT_FALSE(objects->objects().back().carried);
}
