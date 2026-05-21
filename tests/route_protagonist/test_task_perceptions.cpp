#include "brain/ProtagonistBrain.h"
#include "core/agent/Agent.h"
#include "core/world/World2D.h"
#include "perception/WaterPerception.h"
#include "perception/WorksitePerception.h"
#include "world/BaseLayer.h"
#include "world/WaterLayer.h"
#include "world/WorksiteLayer.h"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>

namespace {

using neuro::AgentId;
using neuro::GenomeId;
using neuro::Vec2;
using neuro::World2D;
using neuro::WorldId;
using neuro::routes::protagonist::ProtagonistBrain;
using neuro::routes::protagonist::WaterPerception;
using neuro::routes::protagonist::WorksitePerception;

std::unique_ptr<neuro::Agent> makeStubAgent(AgentId id, Vec2 pos) {
    auto brain = std::make_unique<ProtagonistBrain>(
        8,
        8,
        4,
        8,
        2,
        3,
        static_cast<std::uint32_t>(id.value + 17),
        false);
    return std::make_unique<neuro::Agent>(
        id,
        GenomeId{static_cast<std::uint64_t>(id.value)},
        std::move(brain),
        pos,
        1000.0f);
}

}  // namespace

TEST(TaskPerceptions, WaterCanRequireVisibilityInsteadOfGlobalDirection) {
    World2D world(WorldId{1}, Vec2{200.0f, 200.0f});
    world.addLayer(std::make_unique<neuro::routes::protagonist::BaseLayer>(
        Vec2{100.0f, 100.0f}, 18.0f, 8.0f, 10.0f, 14.0f, -8.0f));
    world.addLayer(std::make_unique<neuro::routes::protagonist::WaterLayer>(
        1, 30.0f, 30.0f, 5.0f, 3.0f, 24.0f, 123u));

    auto agent = makeStubAgent(AgentId{1}, Vec2{0.0f, 0.0f});
    std::array<float, WaterPerception::kDim> omniscient{};
    std::array<float, WaterPerception::kDim> hidden{};

    WaterPerception(36.0f, false).sense(*agent, world, omniscient);
    WaterPerception(36.0f, true).sense(*agent, world, hidden);

    EXPECT_GT(std::abs(omniscient[0]) + std::abs(omniscient[1]), 0.01f);
    EXPECT_FLOAT_EQ(omniscient[4], 1.0f);
    EXPECT_FLOAT_EQ(hidden[0], 0.0f);
    EXPECT_FLOAT_EQ(hidden[1], 0.0f);
    EXPECT_FLOAT_EQ(hidden[2], 1.0f);
    EXPECT_FLOAT_EQ(hidden[3], 0.0f);
    EXPECT_FLOAT_EQ(hidden[4], 0.0f);
}

TEST(TaskPerceptions, WorksiteCanRequireVisibilityInsteadOfGlobalDirection) {
    World2D world(WorldId{2}, Vec2{200.0f, 200.0f});
    world.addLayer(std::make_unique<neuro::routes::protagonist::BaseLayer>(
        Vec2{100.0f, 100.0f}, 18.0f, 8.0f, 10.0f, 14.0f, -8.0f));
    world.addLayer(std::make_unique<neuro::routes::protagonist::WorksiteLayer>(
        1, 10.0f, 6.0f, 2, 1, 0.25f, 0.03f, 123u));

    auto* worksites = world.getLayer<neuro::routes::protagonist::WorksiteLayer>();
    ASSERT_NE(worksites, nullptr);
    ASSERT_TRUE(worksites->activateNextDormant());

    auto agent = makeStubAgent(AgentId{1}, Vec2{0.0f, 0.0f});
    std::array<float, WorksitePerception::kDim> omniscient{};
    std::array<float, WorksitePerception::kDim> hidden{};

    WorksitePerception(28.0f, false).sense(*agent, world, omniscient);
    WorksitePerception(28.0f, true).sense(*agent, world, hidden);

    EXPECT_GT(std::abs(omniscient[0]) + std::abs(omniscient[1]), 0.01f);
    EXPECT_FLOAT_EQ(hidden[0], 0.0f);
    EXPECT_FLOAT_EQ(hidden[1], 0.0f);
    EXPECT_FLOAT_EQ(hidden[2], 0.0f);
    EXPECT_FLOAT_EQ(hidden[3], 0.0f);
    EXPECT_FLOAT_EQ(hidden[4], 1.0f);
}
