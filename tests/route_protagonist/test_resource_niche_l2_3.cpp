// L2.3 resource niche partitioning v2 (SPECIES_DIFFERENTIATION_SPEC.md § 3):
//   - ResourceLayer plant cells get a `kind` (Grass / Browse / Fruit)
//     sampled from PlantSpawnWeights at spawn time
//   - Corpse cells (addCorpse) keep kind = Meat
//   - tryConsumeTyped returns ConsumeResult with `kind` field
//   - Agent.eatEfficiencyOf(kind) gates per-species energy gain
//     (LargeHerbivore: grass 1.0, browse 0.5, fruit 0.3, meat 0.0)
#include "core/agent/Agent.h"
#include "core/brain/fixed/FixedTopologyBrain.h"
#include "core/brain/fixed/FixedTopologyGenome.h"
#include "core/capability/EatCapability.h"
#include "core/perception/SelfPerception.h"
#include "core/types/Vec2.h"
#include "core/world/World2D.h"
#include "core/world/layers/AgentLayer.h"
#include "core/world/layers/ResourceLayer.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <random>
#include <unordered_set>

namespace {

using neuro::Agent;
using neuro::AgentId;
using neuro::AgentLayer;
using neuro::EatCapability;
using neuro::FixedTopologyBrain;
using neuro::FixedTopologyGenome;
using neuro::GenomeId;
using neuro::ResourceKind;
using neuro::ResourceLayer;
using neuro::SelfPerception;
using neuro::Vec2;
using neuro::World2D;
using neuro::WorldId;

std::unique_ptr<Agent> makeAgent(AgentId id, Vec2 pos, float energy = 100.0f) {
    std::mt19937 rng(static_cast<std::uint32_t>(id.value + 41));
    auto genome = FixedTopologyGenome::random(
        GenomeId{id.value}, /*input_dim=*/2, /*hidden_dim=*/2, /*output_dim=*/1, rng);
    auto brain = std::make_unique<FixedTopologyBrain>(*genome);
    auto agent = std::make_unique<Agent>(
        id, GenomeId{static_cast<std::uint64_t>(id.value)},
        std::move(brain), pos, energy);
    agent->addPerception(std::make_unique<SelfPerception>(200.0f, 5000.0));
    return agent;
}

}  // namespace

// ---- ResourceLayer kind sampling ----

TEST(ResourceNicheL23, DefaultSpawnsAllGrass) {
    World2D world(WorldId{1}, Vec2{60, 60});
    ResourceLayer::PlantSpawnWeights w{};  // default = grass 1, browse 0, fruit 0
    auto layer = std::make_unique<ResourceLayer>(
        100, 10.0f, /*respawn_seconds=*/30.0, /*seed=*/77u, false, 0.0f, false, w);
    auto* layer_raw = layer.get();
    world.addLayer(std::move(layer));
    // onAttach happens in addLayer.
    std::size_t grass_count = 0;
    for (const auto& r : layer_raw->resources()) {
        if (r.kind == ResourceKind::Grass) ++grass_count;
    }
    EXPECT_EQ(grass_count, 100u) << "default weights = pure grass";
}

TEST(ResourceNicheL23, MultiChannelMixSamplesAllKinds) {
    World2D world(WorldId{1}, Vec2{60, 60});
    ResourceLayer::PlantSpawnWeights w{1.0f, 1.0f, 1.0f};
    auto layer = std::make_unique<ResourceLayer>(
        300, 10.0f, 30.0, /*seed=*/13u, false, 0.0f, false, w);
    auto* layer_raw = layer.get();
    world.addLayer(std::move(layer));

    std::unordered_set<ResourceKind> seen;
    for (const auto& r : layer_raw->resources()) {
        seen.insert(r.kind);
    }
    EXPECT_TRUE(seen.contains(ResourceKind::Grass));
    EXPECT_TRUE(seen.contains(ResourceKind::Browse));
    EXPECT_TRUE(seen.contains(ResourceKind::Fruit));
    EXPECT_FALSE(seen.contains(ResourceKind::Meat))
        << "plant sampling must never produce Meat (reserved for addCorpse)";
}

TEST(ResourceNicheL23, AddCorpseProducesMeatKind) {
    World2D world(WorldId{1}, Vec2{60, 60});
    auto layer = std::make_unique<ResourceLayer>(0, 10.0f, 30.0, /*seed=*/77u);
    auto* layer_raw = layer.get();
    world.addLayer(std::move(layer));
    layer_raw->addCorpse(Vec2{30, 30}, /*amount=*/15.0f, world);
    ASSERT_EQ(layer_raw->resources().size(), 1u);
    EXPECT_EQ(layer_raw->resources()[0].kind, ResourceKind::Meat);
    EXPECT_TRUE(layer_raw->resources()[0].is_meat);
}

// ---- Eat efficiency by kind ----

TEST(ResourceNicheL23, AgentEatEfficiencyMultipliesGain) {
    World2D world(WorldId{1}, Vec2{60, 60});
    auto resources = std::make_unique<ResourceLayer>(
        0, 10.0f, 30.0, /*seed=*/77u);
    auto* resources_raw = resources.get();
    world.addLayer(std::move(resources));
    auto agents_layer = std::make_unique<AgentLayer>(-1.0e9f, 0.0f, 1);
    world.addLayer(std::move(agents_layer));

    // Two cells side by side: one Browse, one Grass.
    // We can't directly seed a Browse resource via addCorpse (Meat-only)
    // so we use a fresh ResourceLayer with browse-only weights.
    // Easier path: spawn a single Meat (corpse) and verify carnivore
    // gets full gain while pure herbivore gets near zero.
    resources_raw->addCorpse(Vec2{10, 10}, /*amount=*/20.0f, world);

    // Herbivore: meat efficiency 0.0.
    auto herb = makeAgent(AgentId{1}, Vec2{10, 10}, /*energy=*/50.0f);
    Agent::EatEfficiencyByKind herb_eff{};
    herb_eff.grass = 1.0f; herb_eff.browse = 0.5f; herb_eff.fruit = 0.3f; herb_eff.meat = 0.0f;
    herb->setEatEfficiency(herb_eff);
    herb->setDietPreference(0.05f);  // pure herbivore

    EatCapability eat(/*radius=*/4.0f, /*rate_per_sec=*/40.0f);
    const float herb_initial = herb->energy();
    eat.apply(*herb, world, {}, /*dt=*/0.1f);
    EXPECT_NEAR(herb->energy(), herb_initial, 0.5f)
        << "Herbivore eats meat at efficiency 0 -> ~no energy gain (some "
           "dietPreference base may add a tiny amount, but 0 multiplier "
           "should annihilate it).";
}

TEST(ResourceNicheL23, CarnivoreGainsFullFromMeat) {
    World2D world(WorldId{1}, Vec2{60, 60});
    auto resources = std::make_unique<ResourceLayer>(
        0, 10.0f, 30.0, /*seed=*/77u);
    auto* resources_raw = resources.get();
    world.addLayer(std::move(resources));
    auto agents_layer = std::make_unique<AgentLayer>(-1.0e9f, 0.0f, 1);
    world.addLayer(std::move(agents_layer));

    resources_raw->addCorpse(Vec2{10, 10}, /*amount=*/20.0f, world);

    auto carn = makeAgent(AgentId{1}, Vec2{10, 10}, /*energy=*/50.0f);
    Agent::EatEfficiencyByKind carn_eff{};
    carn_eff.grass = 0.0f; carn_eff.browse = 0.0f; carn_eff.fruit = 0.0f; carn_eff.meat = 1.0f;
    carn->setEatEfficiency(carn_eff);
    carn->setDietPreference(0.95f);

    EatCapability eat(4.0f, 40.0f);
    const float c_initial = carn->energy();
    eat.apply(*carn, world, {}, /*dt=*/0.1f);
    EXPECT_GT(carn->energy(), c_initial + 1.0f)
        << "Carnivore eats meat at efficiency 1.0 -> gets significant energy";
}
