#include <gtest/gtest.h>

#include "core/agent/Agent.h"
#include "core/brain/fixed/FixedTopologyBrain.h"
#include "core/brain/fixed/FixedTopologyGenome.h"
#include "core/perception/FoodScentPerception.h"
#include "core/perception/SignalPerception.h"
#include "core/capability/EmitSignalCapability.h"
#include "core/reward/FoodReward.h"
#include "core/reward/SignalBackpropReward.h"
#include "core/world/World2D.h"
#include "core/world/layers/AgentLayer.h"
#include "core/world/layers/ResourceLayer.h"
#include "core/world/layers/terrain/TerrainLayer.h"
#include "core/world/layers/signal/SignalEmitHistory.h"
#include "core/world/layers/signal/SignalLayer.h"

#include <array>
#include <memory>
#include <vector>

namespace {

std::unique_ptr<neuro::Agent> makeAgent(neuro::AgentId agent_id,
                                        neuro::GenomeId genome_id,
                                        neuro::Vec2 position) {
    neuro::FixedTopologyGenome genome(genome_id, 5, 10, 2);
    auto brain = std::make_unique<neuro::FixedTopologyBrain>(genome);
    return std::make_unique<neuro::Agent>(agent_id, genome_id, std::move(brain), position, 10.0f);
}

}  // namespace

TEST(Comm, FoodScentPerceptionIsMirrorInvariantWithoutDirectionLeak) {
    neuro::World2D world(neuro::WorldId{1}, {100.0f, 100.0f});
    world.addLayer(std::make_unique<neuro::ResourceLayer>(2, 1.0f, 10, 1));

    auto* resources = world.getLayer<neuro::ResourceLayer>();
    ASSERT_NE(resources, nullptr);

    auto& mutable_resources = const_cast<std::vector<neuro::ResourceLayer::Resource>&>(resources->resources());
    ASSERT_EQ(mutable_resources.size(), 2u);
    mutable_resources[0].position = {30.0f, 50.0f};
    mutable_resources[0].amount = 1.0f;
    mutable_resources[0].max_amount = 1.0f;
    mutable_resources[0].available = true;
    mutable_resources[1].position = {70.0f, 50.0f};
    mutable_resources[1].amount = 1.0f;
    mutable_resources[1].max_amount = 1.0f;
    mutable_resources[1].available = true;

    neuro::FoodScentPerception perception;
    auto left_agent = makeAgent(neuro::AgentId{1}, neuro::GenomeId{1}, {40.0f, 50.0f});
    auto right_agent = makeAgent(neuro::AgentId{2}, neuro::GenomeId{2}, {60.0f, 50.0f});

    std::array<float, neuro::FoodScentPerception::kDim> left_output{};
    std::array<float, neuro::FoodScentPerception::kDim> right_output{};

    perception.sense(*left_agent, world, std::span<float>(left_output.data(), left_output.size()));
    perception.sense(*right_agent, world, std::span<float>(right_output.data(), right_output.size()));

    EXPECT_NEAR(left_output[0], right_output[0], 1e-6f);
    EXPECT_NEAR(left_output[1], right_output[1], 1e-6f);
}

TEST(Comm, SignalBackpropUsesRewardTimePositionAndTick) {
    neuro::World2D world(neuro::WorldId{2}, {100.0f, 100.0f});
    world.addLayer(std::make_unique<neuro::AgentLayer>(-100.0f, 0.0f));

    auto* agent_layer = world.getLayer<neuro::AgentLayer>();
    ASSERT_NE(agent_layer, nullptr);

    auto source_a = makeAgent(neuro::AgentId{10}, neuro::GenomeId{10}, {10.0f, 10.0f});
    auto* source_a_ptr = source_a.get();
    agent_layer->addAgent(std::move(source_a), world);

    auto source_b = makeAgent(neuro::AgentId{11}, neuro::GenomeId{11}, {80.0f, 80.0f});
    auto* source_b_ptr = source_b.get();
    agent_layer->addAgent(std::move(source_b), world);

    auto receiver = makeAgent(neuro::AgentId{12}, neuro::GenomeId{12}, {10.0f, 10.0f});
    auto* receiver_ptr = receiver.get();
    agent_layer->addAgent(std::move(receiver), world);

    neuro::SignalEmitHistory history(16);
    history.record(source_a_ptr->id(), 0, {10.0f, 10.0f}, 0, 1.0f);
    history.record(source_b_ptr->id(), 0, {80.0f, 80.0f}, 9, 1.0f);

    neuro::SignalBackpropReward backprop(history, 1.0f, 3, 6.0f);
    backprop.recordReceiverReward(receiver_ptr->id(), {10.0f, 10.0f}, 4.0, 1);

    receiver_ptr->setPosition({80.0f, 80.0f});
    backprop.applyBackprop(world, 9);

    EXPECT_NEAR(source_a_ptr->fitness(), 4.0, 1e-9);
    EXPECT_DOUBLE_EQ(source_b_ptr->fitness(), 0.0);
}

TEST(Comm, FoodRewardIgnoresSelfSignalAndRewardsOtherSignal) {
    neuro::World2D world(neuro::WorldId{3}, {100.0f, 100.0f});
    world.addLayer(std::make_unique<neuro::SignalLayer>(neuro::Vec2{100.0f, 100.0f}, 32, 2, 0.1f, 0.0f));

    auto* signal = world.getLayer<neuro::SignalLayer>();
    ASSERT_NE(signal, nullptr);

    auto agent = makeAgent(neuro::AgentId{21}, neuro::GenomeId{21}, {50.0f, 50.0f});
    agent->beginTick();
    agent->noteFoodEaten(1.0f);

    neuro::FoodReward reward(10.0);
    const double baseline = reward.evaluate(*agent, world, 1.0f);

    signal->history().clear();
    signal->emitWithHistory(agent->id(), agent->position(), std::vector<float>{1.0f, 0.0f}, world.currentTick());
    const double self_signal_reward = reward.evaluate(*agent, world, 1.0f);

    signal->history().clear();
    signal->emitWithHistory(neuro::AgentId{99}, agent->position(), std::vector<float>{1.0f, 0.0f}, world.currentTick());
    const double other_signal_reward = reward.evaluate(*agent, world, 1.0f);

    EXPECT_DOUBLE_EQ(self_signal_reward, baseline);
    EXPECT_DOUBLE_EQ(other_signal_reward, baseline + 1.0);
}

TEST(Comm, SignalEmissionCostScalesWithIntensity) {
    neuro::World2D world(neuro::WorldId{4}, {100.0f, 100.0f});
    world.addLayer(std::make_unique<neuro::SignalLayer>(neuro::Vec2{100.0f, 100.0f}, 32, 2, 0.1f, 0.0f));

    neuro::EmitSignalCapability capability(2, 0.5f);

    auto low_agent = makeAgent(neuro::AgentId{31}, neuro::GenomeId{31}, {50.0f, 50.0f});
    auto high_agent = makeAgent(neuro::AgentId{32}, neuro::GenomeId{32}, {50.0f, 50.0f});

    const float low_before = low_agent->energy();
    const float high_before = high_agent->energy();

    const std::array<float, 2> low_signal{0.2f, 0.0f};
    const std::array<float, 2> high_signal{1.0f, 0.0f};
    capability.apply(*low_agent, world, std::span<const float>(low_signal.data(), low_signal.size()), 1.0f);
    capability.apply(*high_agent, world, std::span<const float>(high_signal.data(), high_signal.size()), 1.0f);

    EXPECT_LT(low_agent->energy(), low_before);
    EXPECT_LT(high_agent->energy(), high_before);
    EXPECT_LT(high_agent->energy(), low_agent->energy());
}

TEST(Comm, SignalSamplingWeightsNearbyCellsMoreStrongly) {
    neuro::SignalLayer layer(neuro::Vec2{100.0f, 100.0f}, 10, 1, 0.0f, 0.0f);
    neuro::World2D world(neuro::WorldId{5}, {100.0f, 100.0f});
    layer.onAttach(world);

    layer.emit({50.0f, 50.0f}, std::vector<float>{1.0f});

    const auto near_sample = layer.sample({50.0f, 50.0f}, 20.0f);
    const auto far_sample = layer.sample({68.0f, 50.0f}, 20.0f);

    ASSERT_EQ(near_sample.size(), 1u);
    ASSERT_EQ(far_sample.size(), 1u);
    EXPECT_GT(near_sample[0], far_sample[0]);
}

TEST(Comm, SignalPerceptionExposesCurrentLevelAndTrend) {
    neuro::World2D world(neuro::WorldId{6}, {100.0f, 100.0f});
    world.addLayer(std::make_unique<neuro::SignalLayer>(neuro::Vec2{100.0f, 100.0f}, 32, 2, 0.0f, 0.0f));

    auto* signal_layer = world.getLayer<neuro::SignalLayer>();
    ASSERT_NE(signal_layer, nullptr);

    auto agent = makeAgent(neuro::AgentId{41}, neuro::GenomeId{41}, {50.0f, 50.0f});
    neuro::SignalPerception perception(2, 12.0f);
    std::array<float, 4> first_output{};
    std::array<float, 4> second_output{};

    perception.sense(*agent, world, std::span<float>(first_output.data(), first_output.size()));
    signal_layer->emit({50.0f, 50.0f}, std::vector<float>{1.0f, 0.0f});
    perception.sense(*agent, world, std::span<float>(second_output.data(), second_output.size()));

    EXPECT_EQ(perception.dim(), 4u);
    EXPECT_NEAR(first_output[0], 0.0f, 1e-6f);
    EXPECT_GT(second_output[0], first_output[0]);
    EXPECT_GT(second_output[1], 0.0f);
}

TEST(Comm, SignalPerceptionUsesRelativeTerrainHeightThreshold) {
    neuro::World2D baseline_world(neuro::WorldId{7}, {100.0f, 100.0f});
    baseline_world.addLayer(std::make_unique<neuro::SignalLayer>(neuro::Vec2{100.0f, 100.0f}, 32, 2, 0.0f, 0.0f));

    neuro::World2D terrain_world(neuro::WorldId{8}, {100.0f, 100.0f});
    terrain_world.addLayer(std::make_unique<neuro::SignalLayer>(neuro::Vec2{100.0f, 100.0f}, 32, 2, 0.0f, 0.0f));
    neuro::TerrainParams terrain_params;
    terrain_params.seed = 42;
    terrain_params.grid_resolution = 8;
    terrain_params.height_scale = 60.0f;
    terrain_world.addLayer(std::make_unique<neuro::TerrainLayer>(neuro::Vec2{100.0f, 100.0f}, terrain_params));

    auto* terrain = terrain_world.getLayer<neuro::TerrainLayer>();
    ASSERT_NE(terrain, nullptr);
    for (std::size_t y = 0; y < terrain->heightFieldMut().height(); ++y) {
        for (std::size_t x = 0; x < terrain->heightFieldMut().width(); ++x) {
            terrain->heightFieldMut().at(x, y) = 25.0f;
            terrain->caveMaskMut().set(x, y, false);
        }
    }

    auto baseline_agent = makeAgent(neuro::AgentId{51}, neuro::GenomeId{51}, {50.0f, 50.0f});
    auto terrain_agent = makeAgent(neuro::AgentId{52}, neuro::GenomeId{52}, {50.0f, 50.0f});
    neuro::SignalPerception baseline_perception(2, 12.0f);
    neuro::SignalPerception terrain_perception(2, 12.0f);
    std::array<float, 4> baseline_output{};
    std::array<float, 4> terrain_output{};

    baseline_world.getLayer<neuro::SignalLayer>()->emit({50.0f, 50.0f}, std::vector<float>{1.0f, 0.0f});
    terrain_world.getLayer<neuro::SignalLayer>()->emit({50.0f, 50.0f}, std::vector<float>{1.0f, 0.0f});

    baseline_perception.sense(*baseline_agent, baseline_world, std::span<float>(baseline_output.data(), baseline_output.size()));
    terrain_perception.sense(*terrain_agent, terrain_world, std::span<float>(terrain_output.data(), terrain_output.size()));

    EXPECT_NEAR(terrain_output[0], baseline_output[0], 1e-6f);
}

TEST(Comm, SignalPerceptionCanDisableTerrainModulation) {
    neuro::World2D baseline_world(neuro::WorldId{9}, {100.0f, 100.0f});
    baseline_world.addLayer(std::make_unique<neuro::SignalLayer>(neuro::Vec2{100.0f, 100.0f}, 32, 2, 0.0f, 0.0f));

    neuro::World2D terrain_world(neuro::WorldId{10}, {100.0f, 100.0f});
    terrain_world.addLayer(std::make_unique<neuro::SignalLayer>(neuro::Vec2{100.0f, 100.0f}, 32, 2, 0.0f, 0.0f));
    neuro::TerrainParams terrain_params;
    terrain_params.seed = 42;
    terrain_params.grid_resolution = 8;
    terrain_params.height_scale = 60.0f;
    terrain_world.addLayer(std::make_unique<neuro::TerrainLayer>(neuro::Vec2{100.0f, 100.0f}, terrain_params));

    auto* terrain = terrain_world.getLayer<neuro::TerrainLayer>();
    ASSERT_NE(terrain, nullptr);
    for (std::size_t y = 0; y < terrain->heightFieldMut().height(); ++y) {
        for (std::size_t x = 0; x < terrain->heightFieldMut().width(); ++x) {
            terrain->heightFieldMut().at(x, y) = 45.0f;
            terrain->caveMaskMut().set(x, y, false);
        }
    }

    auto baseline_agent = makeAgent(neuro::AgentId{61}, neuro::GenomeId{61}, {50.0f, 50.0f});
    auto terrain_agent = makeAgent(neuro::AgentId{62}, neuro::GenomeId{62}, {50.0f, 50.0f});
    neuro::SignalPerception baseline_perception(2, 12.0f);
    neuro::SignalPerception terrain_perception(2, 12.0f, false);
    std::array<float, 4> baseline_output{};
    std::array<float, 4> terrain_output{};

    baseline_world.getLayer<neuro::SignalLayer>()->emit({50.0f, 50.0f}, std::vector<float>{1.0f, 0.0f});
    terrain_world.getLayer<neuro::SignalLayer>()->emit({50.0f, 50.0f}, std::vector<float>{1.0f, 0.0f});

    baseline_perception.sense(*baseline_agent, baseline_world, std::span<float>(baseline_output.data(), baseline_output.size()));
    terrain_perception.sense(*terrain_agent, terrain_world, std::span<float>(terrain_output.data(), terrain_output.size()));

    EXPECT_NEAR(terrain_output[0], baseline_output[0], 1e-6f);
}
