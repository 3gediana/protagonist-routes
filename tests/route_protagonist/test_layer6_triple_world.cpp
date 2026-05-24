// Layer 6 R2 skeleton tests:
//   - MigrationScheduler::extractTopK picks the right genomes
//   - MigrationScheduler::seedFromCandidates appends correctly
//   - TripleWorldRuntime::promoteSandboxToBorderland / promoteBorderlandToMain
//     round-trip top-K from one world's pool to the next
//   - WorldRole enum naming
//
// The full TripleWorldRuntime (3 World2D instances + tick loop +
// disk persistence) is deferred per LAYER6_THREE_WORLDS_SPEC § 6
// priority list. These tests lock the migration arithmetic so that
// when the next session wires it into ProtagonistEcologyRuntime, the
// behavior is already covered.

#include "core/brain/neat/NeatGenome.h"
#include "runtime/MigrationScheduler.h"
#include "runtime/TripleWorldRuntime.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace {

using neuro::ConnectionGene;
using neuro::GenomeId;
using neuro::IGenome;
using neuro::NeatGenome;
using neuro::NodeGene;
using neuro::NodeGeneType;
using neuro::NodeId;
using neuro::routes::protagonist::MigrationCandidate;
using neuro::routes::protagonist::MigrationScheduler;
using neuro::routes::protagonist::TripleWorldRuntime;
using neuro::routes::protagonist::WorldRole;
using neuro::routes::protagonist::worldRoleName;

std::unique_ptr<IGenome> makeTinyGenome(GenomeId id) {
    std::vector<NodeGene> nodes;
    nodes.push_back(NodeGene{static_cast<NodeId>(1), NodeGeneType::Input,  0.0f, 1.0f, 1.0f});
    nodes.push_back(NodeGene{static_cast<NodeId>(2), NodeGeneType::Output, 0.0f, 1.0f, 1.0f});
    std::vector<ConnectionGene> conns;
    conns.push_back(ConnectionGene{static_cast<NodeId>(1), static_cast<NodeId>(2),
                                   0.1f, true, neuro::kInvalidInnovationId});
    return std::make_unique<NeatGenome>(id, /*input=*/1, /*output=*/1,
                                        std::move(nodes), std::move(conns), 1.0f);
}

}  // namespace

TEST(Layer6Migration, WorldRoleName) {
    EXPECT_STREQ(worldRoleName(WorldRole::TrainingSandbox), "training_sandbox");
    EXPECT_STREQ(worldRoleName(WorldRole::Borderland),      "borderland");
    EXPECT_STREQ(worldRoleName(WorldRole::MainWorld),       "main_world");
}

TEST(Layer6Migration, ExtractTopKDescendingByFitness) {
    std::vector<std::unique_ptr<IGenome>> genomes;
    for (std::uint64_t i = 0; i < 5; ++i) {
        genomes.push_back(makeTinyGenome(GenomeId{i + 100u}));
    }
    std::vector<double> fits = {1.0, 4.0, 2.0, 5.0, 3.0};
    std::vector<std::uint64_t> lineages = {10, 11, 12, 13, 14};

    auto candidates = MigrationScheduler::extractTopK(genomes, fits, lineages, 3);
    ASSERT_EQ(candidates.size(), 3u);
    // Sorted: idx 3 (fit=5), idx 1 (fit=4), idx 4 (fit=3)
    EXPECT_DOUBLE_EQ(candidates[0].fitness, 5.0);
    EXPECT_EQ(candidates[0].lineage_id, 13u);
    EXPECT_EQ(candidates[0].genome->id().value, 103u);
    EXPECT_DOUBLE_EQ(candidates[1].fitness, 4.0);
    EXPECT_EQ(candidates[1].lineage_id, 11u);
    EXPECT_DOUBLE_EQ(candidates[2].fitness, 3.0);
    EXPECT_EQ(candidates[2].lineage_id, 14u);

    // Source genomes must NOT have been moved out — extractTopK clones.
    for (const auto& g : genomes) ASSERT_NE(g, nullptr);
}

TEST(Layer6Migration, ExtractTopKHandlesKLargerThanSource) {
    std::vector<std::unique_ptr<IGenome>> genomes;
    genomes.push_back(makeTinyGenome(GenomeId{1}));
    genomes.push_back(makeTinyGenome(GenomeId{2}));
    std::vector<double> fits = {0.5, 0.7};
    std::vector<std::uint64_t> lineages = {1, 2};

    auto candidates = MigrationScheduler::extractTopK(genomes, fits, lineages, 99);
    EXPECT_EQ(candidates.size(), 2u);
}

TEST(Layer6Migration, ExtractTopKThrowsOnSizeMismatch) {
    std::vector<std::unique_ptr<IGenome>> genomes;
    genomes.push_back(makeTinyGenome(GenomeId{1}));
    std::vector<double> fits = {0.1, 0.2};  // mismatch
    std::vector<std::uint64_t> lineages = {1};
    EXPECT_THROW(MigrationScheduler::extractTopK(genomes, fits, lineages, 1),
                 std::runtime_error);
}

TEST(Layer6Migration, SeedFromCandidatesAppends) {
    std::vector<MigrationCandidate> cs;
    cs.push_back(MigrationCandidate{makeTinyGenome(GenomeId{1}), 1.0, 100u});
    cs.push_back(MigrationCandidate{makeTinyGenome(GenomeId{2}), 2.0, 200u});

    std::vector<std::unique_ptr<IGenome>> tgt_pop;
    std::vector<std::uint64_t> tgt_lineages;
    // Pre-existing entries should NOT be cleared.
    tgt_pop.push_back(makeTinyGenome(GenomeId{99}));
    tgt_lineages.push_back(99u);

    MigrationScheduler::seedFromCandidates(std::move(cs), tgt_pop, tgt_lineages);
    ASSERT_EQ(tgt_pop.size(), 3u);
    ASSERT_EQ(tgt_lineages.size(), 3u);
    EXPECT_EQ(tgt_pop[0]->id().value, 99u);
    EXPECT_EQ(tgt_pop[1]->id().value, 1u);
    EXPECT_EQ(tgt_pop[2]->id().value, 2u);
    EXPECT_EQ(tgt_lineages[0], 99u);
    EXPECT_EQ(tgt_lineages[1], 100u);
    EXPECT_EQ(tgt_lineages[2], 200u);
}

TEST(Layer6Migration, TripleWorldPromoteSandboxToBorderland) {
    TripleWorldRuntime trio;

    std::vector<std::unique_ptr<IGenome>> sandbox_pop;
    for (std::uint64_t i = 0; i < 4; ++i) {
        sandbox_pop.push_back(makeTinyGenome(GenomeId{i + 1u}));
    }
    std::vector<std::uint64_t> sandbox_lin = {1u, 2u, 3u, 4u};
    trio.seedSandboxPool(/*archetype=*/0, std::move(sandbox_pop), std::move(sandbox_lin));

    std::vector<double> fits = {0.5, 0.9, 0.8, 0.1};
    const std::size_t promoted = trio.promoteSandboxToBorderland(0, fits, 2);
    EXPECT_EQ(promoted, 2u);

    const auto& border = trio.borderland().pools_by_archetype.at(0);
    ASSERT_EQ(border.genomes.size(), 2u);
    ASSERT_EQ(border.lineage_ids.size(), 2u);
    // Top fitness was 0.9 (idx 1, lineage 2), then 0.8 (idx 2, lineage 3).
    EXPECT_EQ(border.genomes[0]->id().value, 2u);
    EXPECT_EQ(border.lineage_ids[0], 2u);
    EXPECT_EQ(border.genomes[1]->id().value, 3u);
    EXPECT_EQ(border.lineage_ids[1], 3u);

    // Sandbox must remain intact — clones, not moves.
    EXPECT_EQ(trio.sandbox().pools_by_archetype.at(0).genomes.size(), 4u);
}

TEST(Layer6Migration, TripleWorldPromoteBorderlandToMain) {
    TripleWorldRuntime trio;

    // Manually seed borderland (skip sandbox stage) for isolation.
    auto& border = trio.borderland().pools_by_archetype;
    border.resize(1);
    for (std::uint64_t i = 0; i < 3; ++i) {
        border[0].genomes.push_back(makeTinyGenome(GenomeId{i + 50u}));
        border[0].lineage_ids.push_back(i + 50u);
    }
    std::vector<double> fits = {1.0, 3.0, 2.0};
    const std::size_t promoted = trio.promoteBorderlandToMain(0, fits, 1);
    EXPECT_EQ(promoted, 1u);

    const auto& main_pool = trio.mainWorld().pools_by_archetype.at(0);
    ASSERT_EQ(main_pool.genomes.size(), 1u);
    EXPECT_EQ(main_pool.genomes[0]->id().value, 51u);  // fit=3.0 winner
    EXPECT_EQ(main_pool.lineage_ids[0], 51u);
}

// L6 R2 stage 2: promoteExternalToBorderland uses the runtime's live
// sandbox pool directly (no internal mirror), and stashes fitnesses
// inside the borderland slot for a later stored-fitness promote.
TEST(Layer6Migration, TripleWorldPromoteExternalAndStoredBorderToMain) {
    TripleWorldRuntime trio;

    std::vector<std::unique_ptr<IGenome>> external_pop;
    for (std::uint64_t i = 0; i < 4; ++i) {
        external_pop.push_back(makeTinyGenome(GenomeId{i + 200u}));
    }
    std::vector<double> fits = {0.3, 0.9, 0.7, 0.5};
    std::vector<std::uint64_t> lineages = {201u, 202u, 203u, 204u};

    const std::size_t n = trio.promoteExternalToBorderland(0, external_pop, fits, lineages, 2);
    EXPECT_EQ(n, 2u);

    // External pop is untouched (extractTopK clones).
    EXPECT_EQ(external_pop.size(), 4u);
    for (const auto& g : external_pop) ASSERT_NE(g, nullptr);

    const auto& border = trio.borderland().pools_by_archetype.at(0);
    ASSERT_EQ(border.genomes.size(), 2u);
    ASSERT_EQ(border.fitnesses.size(), 2u);
    EXPECT_DOUBLE_EQ(border.fitnesses[0], 0.9);
    EXPECT_DOUBLE_EQ(border.fitnesses[1], 0.7);
    // GenomeId{i + 200u} -> idx 1 (fit=0.9) is genome 201, idx 2 (fit=0.7) is 202.
    EXPECT_EQ(border.genomes[0]->id().value, 201u);
    EXPECT_EQ(border.genomes[1]->id().value, 202u);
    EXPECT_EQ(border.lineage_ids[0], 202u);
    EXPECT_EQ(border.lineage_ids[1], 203u);

    // Now the stored-fitness overload picks the top-1 (fit=0.9 winner, genome 201).
    const std::size_t main_promoted = trio.promoteBorderlandToMain(0, /*k=*/1);
    EXPECT_EQ(main_promoted, 1u);
    const auto& main_pool = trio.mainWorld().pools_by_archetype.at(0);
    ASSERT_EQ(main_pool.genomes.size(), 1u);
    EXPECT_EQ(main_pool.genomes[0]->id().value, 201u);
    EXPECT_DOUBLE_EQ(main_pool.fitnesses[0], 0.9);
}
