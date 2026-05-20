#include <gtest/gtest.h>

#include "core/world/layers/terrain/HeightField.h"
#include "core/world/layers/terrain/CaveMask.h"
#include "core/world/layers/terrain/TerrainGenerator.h"
#include "core/world/layers/terrain/TerrainLayer.h"
#include "core/perception/TerrainPerception.h"
#include "core/agent/Agent.h"
#include "core/brain/fixed/FixedTopologyBrain.h"
#include "core/brain/fixed/FixedTopologyGenome.h"
#include "core/world/World2D.h"
#include "core/world/layers/AgentLayer.h"

#include <array>
#include <random>

TEST(HeightField, BilinearInterpolationAtGridPoint) {
    neuro::HeightField hf(4, 4, 10.0f);
    hf.at(1, 1) = 5.0f;
    EXPECT_FLOAT_EQ(hf.heightAt(10.0f, 10.0f), 5.0f);
}

TEST(HeightField, SlopeIsNonZeroOnGradient) {
    neuro::HeightField hf(4, 4, 10.0f);
    hf.at(0, 0) = 0.0f;
    hf.at(1, 0) = 10.0f;
    hf.at(2, 0) = 20.0f;
    const float sx = hf.slopeX(10.0f, 0.0f);
    EXPECT_GT(sx, 0.0f);
}

TEST(CaveMask, SetAndGet) {
    neuro::CaveMask mask(4, 4, 10.0f);
    mask.set(1, 1, true);
    EXPECT_TRUE(mask.at(1, 1));
    EXPECT_FALSE(mask.at(0, 0));
}

TEST(CaveMask, IsCaveWorldCoord) {
    neuro::CaveMask mask(4, 4, 10.0f);
    mask.set(1, 1, true);
    EXPECT_TRUE(mask.isCave(10.0f, 10.0f));
    EXPECT_FALSE(mask.isCave(0.0f, 0.0f));
}

TEST(TerrainGenerator, GeneratesNonFlatHeightField) {
    neuro::TerrainParams params;
    params.seed = 42;
    neuro::HeightField hf(64, 64, 1.25f);
    neuro::TerrainGenerator::generateHeightField(hf, params, 42);
    EXPECT_NE(hf.minHeight(), hf.maxHeight());
}

TEST(TerrainLayer, HeightQueryInRange) {
    neuro::TerrainParams params;
    params.seed = 42;
    params.grid_resolution = 32;
    neuro::TerrainLayer layer(neuro::Vec2{80.0f, 80.0f}, params);
    const float h = layer.heightAt(40.0f, 40.0f);
    EXPECT_GE(h, 0.0f);
    EXPECT_LE(h, params.height_scale);
}

TEST(TerrainLayer, SlopeQueryReturnsVec2) {
    neuro::TerrainParams params;
    params.seed = 42;
    params.grid_resolution = 32;
    neuro::TerrainLayer layer(neuro::Vec2{80.0f, 80.0f}, params);
    const neuro::Vec2 slope = layer.slopeAt(40.0f, 40.0f);
    (void)slope;
}

TEST(TerrainPerception, DimIs5) {
    neuro::TerrainPerception tp(30.0f);
    EXPECT_EQ(tp.dim(), 5u);
}

TEST(TerrainPerception, UsesLiveTerrainHeightScaleForNormalization) {
    neuro::World2D world(neuro::WorldId{2}, {80.0f, 80.0f});
    neuro::TerrainParams params;
    params.seed = 42;
    params.grid_resolution = 8;
    params.height_scale = 60.0f;
    world.addLayer(std::make_unique<neuro::TerrainLayer>(neuro::Vec2{80.0f, 80.0f}, params));

    auto* terrain = world.getLayer<neuro::TerrainLayer>();
    ASSERT_NE(terrain, nullptr);
    for (std::size_t y = 0; y < terrain->heightFieldMut().height(); ++y) {
        for (std::size_t x = 0; x < terrain->heightFieldMut().width(); ++x) {
            terrain->heightFieldMut().at(x, y) = 30.0f;
        }
    }

    neuro::FixedTopologyGenome genome(neuro::GenomeId{3}, 5, 10, 2);
    auto brain = std::make_unique<neuro::FixedTopologyBrain>(genome);
    neuro::Agent agent(neuro::AgentId{3}, neuro::GenomeId{3}, std::move(brain), {20.0f, 20.0f}, 10.0f);
    neuro::TerrainPerception perception(30.0f);
    std::array<float, neuro::TerrainPerception::kDim> output{};

    perception.sense(agent, world, std::span<float>(output.data(), output.size()));

    EXPECT_NEAR(output[0], 0.5f, 1e-6f);
}
