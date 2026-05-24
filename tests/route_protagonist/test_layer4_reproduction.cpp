// Layer 4 (Final Blueprint) regression tests for the 2026-05-24 batch:
//   - NeatBrain::writeWeightsBackTo round-trips weights into a genome
//   - PopulationLayer pairs by archetype_tag, not speciesId
//   - Agent::deathCounted one-shot flag
//   - In-world born children entering the cross-episode pool (bridge)
//
// These tests sit at unit-test level: small synthetic worlds with one
// AgentLayer + one PopulationLayer, two adult parents of one
// archetype, force a reproduction tick, then assert the resulting
// child's genome is the parents' archive structure and BornChild is
// queued.

#include "core/agent/Agent.h"
#include "core/brain/neat/NeatBrain.h"
#include "core/brain/neat/NeatGenome.h"
#include "core/types/Vec2.h"
#include "core/world/World2D.h"
#include "core/world/layers/AgentLayer.h"
#include "world/PopulationLayer.h"

#include <gtest/gtest.h>

#include <memory>
#include <random>

namespace {

using neuro::Agent;
using neuro::AgentId;
using neuro::AgentLayer;
using neuro::ConnectionGene;
using neuro::GenomeId;
using neuro::IGenome;
using neuro::NeatBrain;
using neuro::NeatGenome;
using neuro::NodeGene;
using neuro::NodeGeneType;
using neuro::NodeId;
using neuro::Vec2;
using neuro::World2D;
using neuro::routes::protagonist::BornChild;
using neuro::routes::protagonist::PopulationLayer;

// Build a minimal NeatGenome: 2 input nodes, 1 output node, 2
// enabled connections. Weights are 0.1, 0.2 by default.
std::unique_ptr<NeatGenome> makeTinyGenome(GenomeId id,
                                            float w0 = 0.1f,
                                            float w1 = 0.2f) {
    std::vector<NodeGene> nodes;
    nodes.push_back(NodeGene{static_cast<NodeId>(1), NodeGeneType::Input,  0.0f, 1.0f, 1.0f});
    nodes.push_back(NodeGene{static_cast<NodeId>(2), NodeGeneType::Input,  0.0f, 1.0f, 1.0f});
    nodes.push_back(NodeGene{static_cast<NodeId>(3), NodeGeneType::Output, 0.0f, 1.0f, 1.0f});

    std::vector<ConnectionGene> connections;
    connections.push_back(ConnectionGene{
        static_cast<NodeId>(1), static_cast<NodeId>(3), w0, true,
        neuro::kInvalidInnovationId});
    connections.push_back(ConnectionGene{
        static_cast<NodeId>(2), static_cast<NodeId>(3), w1, true,
        neuro::kInvalidInnovationId});

    auto g = std::make_unique<NeatGenome>(
        id,
        /*input_dim=*/2,
        /*output_dim=*/1,
        std::move(nodes),
        std::move(connections),
        /*decision_interval=*/1.0f);
    return g;
}

}  // namespace

// 1. NeatBrain::writeWeightsBackTo writes the brain's current weights
//    (after hebbianUpdate or setWeights) back into a fresh genome clone.
TEST(Layer4ReproductionRegression, NeatBrainWriteWeightsBackTo) {
    auto src_genome = makeTinyGenome(GenomeId{1}, 0.1f, 0.2f);
    NeatBrain brain(*src_genome);

    // Mutate brain weights so we know they differ from genome.
    brain.setWeights({0.7f, -0.4f});

    // Clone the genome and write the brain weights back to the clone.
    auto cloned = src_genome->clone();
    auto* cloned_neat = dynamic_cast<NeatGenome*>(cloned.get());
    ASSERT_NE(cloned_neat, nullptr);

    brain.writeWeightsBackTo(*cloned_neat);

    const auto& conns = cloned_neat->connections();
    ASSERT_EQ(conns.size(), 2u);
    // Connections targeting the same NodeId (3) are stored in genome
    // connection order. writeWeightsBackTo writes the corresponding
    // brain incoming weights for that target, in the same order they
    // were appended in NeatBrain's ctor: conn[0]=src1->3, conn[1]=src2->3
    // -> brain target_node(3).incoming = [(src1, w0=0.7), (src2, w1=-0.4)].
    EXPECT_NEAR(conns[0].weight, 0.7f, 1e-5f);
    EXPECT_NEAR(conns[1].weight, -0.4f, 1e-5f);

    // Source genome must not have been modified by writeWeightsBackTo.
    EXPECT_NEAR(src_genome->connections()[0].weight, 0.1f, 1e-5f);
    EXPECT_NEAR(src_genome->connections()[1].weight, 0.2f, 1e-5f);
}

// 2. PopulationLayer pairs by archetype_tag, not speciesId. Two adults
//    of different archetype_tag but the same (default) speciesId must
//    NOT be selected as a reproduction pair. Spawn callback should not
//    fire.
TEST(Layer4ReproductionRegression, PopulationLayerPairsByArchetypeNotSpecies) {
    World2D world(neuro::WorldId{1}, Vec2{50.0f, 50.0f});
    world.addLayer(std::make_unique<AgentLayer>(/*death_threshold=*/0.0f,
                                                /*base_metabolism=*/0.0f,
                                                /*decision_threads=*/1));
    auto* agent_layer = world.getLayer<AgentLayer>();

    // Two adult background agents with DIFFERENT archetype_tag but same
    // (default) species_id.
    for (int i = 0; i < 2; ++i) {
        auto genome = makeTinyGenome(GenomeId{static_cast<std::uint64_t>(10 + i)});
        auto brain = std::make_unique<NeatBrain>(*genome);
        auto a = std::make_unique<Agent>(
            AgentId{static_cast<std::uint64_t>(100 + i)},
            genome->id(),
            std::move(brain),
            Vec2{10.0f + static_cast<float>(i), 10.0f},
            /*energy=*/120.0f,
            /*exploration_grid_size=*/20,
            /*decision_interval=*/1.0f);
        a->setArchetypeTag(static_cast<std::uint8_t>(i == 0 ? 1 : 2));
        a->setMaxAgeSeconds(1000.0);
        a->setAgeSeconds(500.0);  // mid Adult phase
        a->setLineageId(static_cast<std::uint64_t>(i + 1));
        a->setGenomeArchive(genome->clone());
        agent_layer->addAgent(std::move(a), world);
    }

    // Spawn callback should NEVER be called because the two agents have
    // different archetype_tag — that's the regression we want to lock.
    bool spawn_fired = false;
    PopulationLayer::Coefficients c;
    c.reproduction_check_interval = 0.1;
    c.reproduction_radius = 10.0f;
    c.reproduction_min_energy = 50.0f;
    c.max_living_agents = 10;
    c.max_spawns_per_tick = 1;
    auto cb = [&spawn_fired](const Agent&, const Agent&, AgentId, GenomeId) {
        spawn_fired = true;
        return std::unique_ptr<Agent>{};
    };
    auto pop_layer_owned = std::make_unique<PopulationLayer>(c, std::move(cb));
    world.addLayer(std::move(pop_layer_owned));

    world.tick(0.2f);  // > reproduction_check_interval
    EXPECT_FALSE(spawn_fired);
    EXPECT_EQ(agent_layer->size(), 2u);
}

// 3. Same archetype → spawn callback fires AND BornChild is queued.
TEST(Layer4ReproductionRegression, SameArchetypePairs_AndBornChildQueued) {
    World2D world(neuro::WorldId{2}, Vec2{50.0f, 50.0f});
    world.addLayer(std::make_unique<AgentLayer>(/*death_threshold=*/0.0f,
                                                /*base_metabolism=*/0.0f,
                                                /*decision_threads=*/1));
    auto* agent_layer = world.getLayer<AgentLayer>();

    for (int i = 0; i < 2; ++i) {
        auto genome = makeTinyGenome(GenomeId{static_cast<std::uint64_t>(20 + i)});
        auto brain = std::make_unique<NeatBrain>(*genome);
        auto a = std::make_unique<Agent>(
            AgentId{static_cast<std::uint64_t>(200 + i)},
            genome->id(),
            std::move(brain),
            Vec2{15.0f + static_cast<float>(i), 15.0f},
            /*energy=*/120.0f,
            /*exploration_grid_size=*/20,
            /*decision_interval=*/1.0f);
        a->setArchetypeTag(7u);  // Omnivore-ish; same for both
        a->setMaxAgeSeconds(1000.0);
        a->setAgeSeconds(500.0);
        a->setLineageId(static_cast<std::uint64_t>(i + 1));
        a->setGenomeArchive(genome->clone());
        agent_layer->addAgent(std::move(a), world);
    }

    int spawn_calls = 0;
    PopulationLayer::Coefficients c;
    c.reproduction_check_interval = 0.1;
    c.reproduction_radius = 10.0f;
    c.reproduction_min_energy = 50.0f;
    c.max_living_agents = 10;
    c.max_spawns_per_tick = 1;
    auto cb = [&spawn_calls](const Agent& a, const Agent& /*b*/,
                             AgentId new_aid, GenomeId new_gid) {
        ++spawn_calls;
        auto child_genome = makeTinyGenome(new_gid, 0.5f, 0.6f);
        auto brain = std::make_unique<NeatBrain>(*child_genome);
        auto child = std::make_unique<Agent>(
            new_aid, new_gid, std::move(brain),
            a.position(), 30.0f, 20, 1.0f);
        child->setArchetypeTag(a.archetypeTag());
        child->setGenomeArchive(std::move(child_genome));
        return child;
    };
    auto pop = std::make_unique<PopulationLayer>(c, std::move(cb));
    auto* pop_raw = pop.get();
    world.addLayer(std::move(pop));

    world.tick(0.2f);  // trigger reproduction check
    EXPECT_EQ(spawn_calls, 1);
    EXPECT_EQ(agent_layer->size(), 3u);  // 2 parents + 1 child

    auto births = pop_raw->drainBornChildren();
    ASSERT_EQ(births.size(), 1u);
    EXPECT_EQ(births[0].archetype_tag, 7u);
    EXPECT_NE(births[0].genome_clone, nullptr);
    EXPECT_EQ(births[0].agent_id.value, agent_layer->agents().back()->id().value);
}

// 4. Agent::deathCounted starts false; flips true via markDeathCounted.
TEST(Layer4ReproductionRegression, AgentDeathCountedFlag) {
    auto genome = makeTinyGenome(GenomeId{99});
    auto brain = std::make_unique<NeatBrain>(*genome);
    Agent agent(AgentId{42}, genome->id(), std::move(brain),
                Vec2{0.0f, 0.0f}, 10.0f, 20, 1.0f);
    EXPECT_FALSE(agent.deathCounted());
    agent.markDeathCounted();
    EXPECT_TRUE(agent.deathCounted());
}

// 5. senseRadiusScale defaults to 1.0; setSenseRadiusScale clamps low values.
TEST(Layer4ReproductionRegression, SenseRadiusScaleSetterClampsLow) {
    auto genome = makeTinyGenome(GenomeId{77});
    auto brain = std::make_unique<NeatBrain>(*genome);
    Agent agent(AgentId{1}, genome->id(), std::move(brain),
                Vec2{0.0f, 0.0f}, 10.0f, 20, 1.0f);
    EXPECT_FLOAT_EQ(agent.senseRadiusScale(), 1.0f);
    agent.setSenseRadiusScale(2.5f);
    EXPECT_FLOAT_EQ(agent.senseRadiusScale(), 2.5f);
    agent.setSenseRadiusScale(0.05f);
    EXPECT_FLOAT_EQ(agent.senseRadiusScale(), 0.1f);  // clamp at 0.1
}
