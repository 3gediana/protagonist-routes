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

// L6 R2 stage 3 commit 1: cullBorderland keeps the top-K by stashed
// fitness and drops the rest; pool tracking arrays stay aligned.
TEST(Layer6Migration, CullBorderlandKeepsTopFitness) {
    TripleWorldRuntime trio;
    std::vector<std::unique_ptr<IGenome>> external_pop;
    for (std::uint64_t i = 0; i < 6; ++i) {
        external_pop.push_back(makeTinyGenome(GenomeId{i + 300u}));
    }
    // Promote all 6 into borderland so we have something to cull.
    std::vector<double> fits = {0.1, 0.9, 0.4, 0.7, 0.3, 0.6};
    std::vector<std::uint64_t> lineages = {310u, 311u, 312u, 313u, 314u, 315u};
    trio.promoteExternalToBorderland(0, external_pop, fits, lineages, 6);
    ASSERT_EQ(trio.borderland().pools_by_archetype.at(0).genomes.size(), 6u);

    // Cull to 3. Should drop the bottom three (fit=0.1, 0.3, 0.4).
    const std::size_t dropped = trio.cullBorderland(0, /*max_pool_size=*/3);
    EXPECT_EQ(dropped, 3u);

    const auto& border = trio.borderland().pools_by_archetype.at(0);
    ASSERT_EQ(border.genomes.size(), 3u);
    ASSERT_EQ(border.fitnesses.size(), 3u);
    ASSERT_EQ(border.lineage_ids.size(), 3u);
    EXPECT_DOUBLE_EQ(border.fitnesses[0], 0.9);
    EXPECT_DOUBLE_EQ(border.fitnesses[1], 0.7);
    EXPECT_DOUBLE_EQ(border.fitnesses[2], 0.6);
    // Lineages must follow the same reordering: 0.9->lineage 311, 0.7->313, 0.6->315.
    EXPECT_EQ(border.lineage_ids[0], 311u);
    EXPECT_EQ(border.lineage_ids[1], 313u);
    EXPECT_EQ(border.lineage_ids[2], 315u);
    // Genomes: fit=0.9 is index 1 of original ⇒ id 301, etc.
    EXPECT_EQ(border.genomes[0]->id().value, 301u);
    EXPECT_EQ(border.genomes[1]->id().value, 303u);
    EXPECT_EQ(border.genomes[2]->id().value, 305u);
}

// L6 R2 stage 3 commit 3: rescueExtinctSpecies refills an empty
// main_world archetype pool from borderland by stashed fitness.
TEST(Layer6Migration, RescueExtinctSpeciesRefillsFromBorderland) {
    TripleWorldRuntime trio;
    std::vector<std::unique_ptr<IGenome>> external_pop;
    for (std::uint64_t i = 0; i < 4; ++i) {
        external_pop.push_back(makeTinyGenome(GenomeId{i + 500u}));
    }
    std::vector<double> fits = {0.1, 0.4, 0.9, 0.2};
    std::vector<std::uint64_t> lineages = {510u, 511u, 512u, 513u};
    trio.promoteExternalToBorderland(/*archetype=*/3, external_pop, fits, lineages, 4);

    // main_world archetype 3 is empty: trigger rescue.
    const std::size_t rescued = trio.rescueExtinctSpecies(/*archetype=*/3, /*k=*/2);
    EXPECT_EQ(rescued, 2u);
    ASSERT_GE(trio.mainWorld().pools_by_archetype.size(), 4u);
    const auto& main = trio.mainWorld().pools_by_archetype.at(3);
    ASSERT_EQ(main.genomes.size(), 2u);
    EXPECT_DOUBLE_EQ(main.fitnesses[0], 0.9);
    EXPECT_DOUBLE_EQ(main.fitnesses[1], 0.4);
    EXPECT_EQ(main.lineage_ids[0], 512u);
    EXPECT_EQ(main.lineage_ids[1], 511u);

    // Borderland keeps its copies (rescue is clone, not move).
    const auto& border = trio.borderland().pools_by_archetype.at(3);
    EXPECT_EQ(border.genomes.size(), 4u);
}

// Rescue is a no-op when main_world isn't actually extinct.
TEST(Layer6Migration, RescueExtinctSpeciesNoopWhenMainNonEmpty) {
    TripleWorldRuntime trio;
    std::vector<std::unique_ptr<IGenome>> external_pop;
    for (std::uint64_t i = 0; i < 2; ++i) {
        external_pop.push_back(makeTinyGenome(GenomeId{i + 600u}));
    }
    std::vector<double> fits = {0.7, 0.8};
    std::vector<std::uint64_t> lineages = {610u, 611u};
    // Populate BOTH borderland and main_world for archetype 1.
    trio.promoteExternalToBorderland(/*archetype=*/1, external_pop, fits, lineages, 2);
    trio.promoteBorderlandToMain(/*archetype=*/1, /*k=*/1);
    ASSERT_FALSE(trio.mainWorld().pools_by_archetype.at(1).genomes.empty());

    const std::size_t rescued = trio.rescueExtinctSpecies(/*archetype=*/1, /*k=*/2);
    EXPECT_EQ(rescued, 0u);
    EXPECT_EQ(trio.mainWorld().pools_by_archetype.at(1).genomes.size(), 1u);
}

// Cull is a no-op when pool already fits (or is smaller than the cap).
TEST(Layer6Migration, CullBorderlandNoopWhenUnderCap) {
    TripleWorldRuntime trio;
    std::vector<std::unique_ptr<IGenome>> external_pop;
    for (std::uint64_t i = 0; i < 3; ++i) {
        external_pop.push_back(makeTinyGenome(GenomeId{i + 400u}));
    }
    std::vector<double> fits = {0.2, 0.5, 0.4};
    std::vector<std::uint64_t> lineages = {410u, 411u, 412u};
    trio.promoteExternalToBorderland(0, external_pop, fits, lineages, 3);

    // Cap = 5, pool size = 3 → no drops, contents untouched.
    EXPECT_EQ(trio.cullBorderland(0, /*max_pool_size=*/5), 0u);
    EXPECT_EQ(trio.cullBorderland(0, /*max_pool_size=*/3), 0u);
    EXPECT_EQ(trio.borderland().pools_by_archetype.at(0).genomes.size(), 3u);
}

// L6 R3 stage 3 commit A: evaluateBorderland walks the borderland pool
// and updates stashed fitness with max(aged_old, fresh). With
// aging_factor=1.0 (default) a stale entry that earned 1000 in the
// sandbox keeps its lead unless the fresh eval also returns >= 1000.
TEST(Layer6Migration, EvaluateBorderlandUpdatesStashedFitness) {
    TripleWorldRuntime trio;
    std::vector<std::unique_ptr<IGenome>> external_pop;
    for (std::uint64_t i = 0; i < 4; ++i) {
        external_pop.push_back(makeTinyGenome(GenomeId{i + 700u}));
    }
    // Stale stashed fitnesses from the sandbox era.
    std::vector<double> fits = {10.0, 20.0, 30.0, 40.0};
    std::vector<std::uint64_t> lineages = {710u, 711u, 712u, 713u};
    trio.promoteExternalToBorderland(/*archetype=*/2, external_pop, fits, lineages, 4);

    // promoteExternalToBorderland sorts by fitness descending, so the
    // borderland pool stores genomes in order [id=703, 702, 701, 700]
    // with fitnesses [40, 30, 20, 10]. Eval callback returns id as
    // fresh fitness, so max(aged_old=fit, fresh=id) = id (all ids
    // exceed their own old fitness).
    const std::size_t evaluated = trio.evaluateBorderland(
        /*archetype=*/2,
        /*eval_fn=*/[](const IGenome& g) {
            return static_cast<double>(g.id().value);
        },
        /*aging_factor=*/1.0);
    EXPECT_EQ(evaluated, 4u);
    const auto& pool = trio.borderland().pools_by_archetype.at(2);
    ASSERT_EQ(pool.fitnesses.size(), 4u);
    EXPECT_DOUBLE_EQ(pool.fitnesses[0], 703.0);
    EXPECT_DOUBLE_EQ(pool.fitnesses[1], 702.0);
    EXPECT_DOUBLE_EQ(pool.fitnesses[2], 701.0);
    EXPECT_DOUBLE_EQ(pool.fitnesses[3], 700.0);
}

// Aging factor < 1 lets fresh evals "catch up" against an old high
// score. With aging=0.5, an old 100 becomes 50; a fresh 60 wins.
TEST(Layer6Migration, EvaluateBorderlandAgingFactor) {
    TripleWorldRuntime trio;
    std::vector<std::unique_ptr<IGenome>> external_pop;
    for (std::uint64_t i = 0; i < 3; ++i) {
        external_pop.push_back(makeTinyGenome(GenomeId{i + 800u}));
    }
    std::vector<double> fits = {100.0, 200.0, 300.0};
    std::vector<std::uint64_t> lineages = {810u, 811u, 812u};
    trio.promoteExternalToBorderland(/*archetype=*/3, external_pop, fits, lineages, 3);

    // promoteExternalToBorderland sorted descending: [id=802(fit=300),
    // id=801(fit=200), id=800(fit=100)]. aging_factor=0.5 → aged old =
    // {150, 100, 50}. fresh callback returns 60 for all.
    //   max(150, 60) = 150
    //   max(100, 60) = 100
    //   max(50, 60)  = 60
    const std::size_t evaluated = trio.evaluateBorderland(
        /*archetype=*/3,
        /*eval_fn=*/[](const IGenome&) { return 60.0; },
        /*aging_factor=*/0.5);
    EXPECT_EQ(evaluated, 3u);
    const auto& pool = trio.borderland().pools_by_archetype.at(3);
    EXPECT_DOUBLE_EQ(pool.fitnesses[0], 150.0);
    EXPECT_DOUBLE_EQ(pool.fitnesses[1], 100.0);
    EXPECT_DOUBLE_EQ(pool.fitnesses[2], 60.0);

    // Null callback or empty pool → no-op (0 returned).
    EXPECT_EQ(trio.evaluateBorderland(/*archetype=*/3,
                                      /*eval_fn=*/nullptr,
                                      /*aging_factor=*/1.0),
              0u);
    // Empty archetype 99 — ensurePool grows the vector but pool stays empty.
    EXPECT_EQ(trio.evaluateBorderland(
                  /*archetype=*/99,
                  /*eval_fn=*/[](const IGenome&) { return 1.0; },
                  /*aging_factor=*/1.0),
              0u);
}

// L6 R3 stage 3 commit C: each evaluateBorderland call bumps
// borderland_eval_count for that archetype's pool. The continuity gate
// in the runtime uses this counter as a 物种延续 condition.
TEST(Layer6Migration, EvaluateBorderlandIncrementsEvalCount) {
    TripleWorldRuntime trio;
    std::vector<std::unique_ptr<IGenome>> external_pop;
    for (std::uint64_t i = 0; i < 2; ++i) {
        external_pop.push_back(makeTinyGenome(GenomeId{i + 900u}));
    }
    std::vector<double> fits = {1.0, 2.0};
    std::vector<std::uint64_t> lineages = {910u, 911u};
    trio.promoteExternalToBorderland(/*archetype=*/5, external_pop, fits, lineages, 2);
    EXPECT_EQ(trio.borderland().pools_by_archetype.at(5).borderland_eval_count, 0u);

    // Five evaluations → counter reaches 5.
    auto identity = [](const IGenome&) { return 0.0; };
    for (int i = 0; i < 5; ++i) {
        trio.evaluateBorderland(/*archetype=*/5, identity, /*aging_factor=*/1.0);
    }
    EXPECT_EQ(trio.borderland().pools_by_archetype.at(5).borderland_eval_count, 5u);

    // A different archetype's counter is independent.
    std::vector<std::unique_ptr<IGenome>> pop6;
    pop6.push_back(makeTinyGenome(GenomeId{950u}));
    std::vector<double> fits6 = {3.0};
    std::vector<std::uint64_t> lin6 = {960u};
    trio.promoteExternalToBorderland(/*archetype=*/6, pop6, fits6, lin6, 1);
    trio.evaluateBorderland(/*archetype=*/6, identity, /*aging_factor=*/1.0);
    EXPECT_EQ(trio.borderland().pools_by_archetype.at(6).borderland_eval_count, 1u);
    EXPECT_EQ(trio.borderland().pools_by_archetype.at(5).borderland_eval_count, 5u);
}

// L6 R3 stage 3 commit J: mutateBorderlandPool walks the pool and
// applies the caller's mutator to every non-null genome. Stashed
// fitness is NOT touched.
TEST(Layer6Migration, MutateBorderlandPoolAppliesToEveryGenome) {
    TripleWorldRuntime trio;
    std::vector<std::unique_ptr<IGenome>> external_pop;
    for (std::uint64_t i = 0; i < 4; ++i) {
        external_pop.push_back(makeTinyGenome(GenomeId{i + 1100u}));
    }
    std::vector<double> fits = {0.5, 0.6, 0.7, 0.8};
    std::vector<std::uint64_t> lineages = {1110u, 1111u, 1112u, 1113u};
    trio.promoteExternalToBorderland(/*archetype=*/2, external_pop, fits, lineages, 4);

    int call_count = 0;
    auto mutator = [&](IGenome& g) {
        ++call_count;
        // Bump connection weight by a known delta; lets us also verify
        // the mutator ran on the actual pool genome (not a copy).
        if (auto* ng = dynamic_cast<NeatGenome*>(&g)) {
            for (auto& c : ng->connections()) c.weight += 0.5f;
        }
    };
    const std::size_t mutated = trio.mutateBorderlandPool(/*archetype=*/2, mutator);
    EXPECT_EQ(mutated, 4u);
    EXPECT_EQ(call_count, 4);

    // Stashed fitness slot should be untouched.
    const auto& pool = trio.borderland().pools_by_archetype.at(2);
    ASSERT_EQ(pool.fitnesses.size(), 4u);
    EXPECT_DOUBLE_EQ(pool.fitnesses[0], 0.8);  // promoteExternalToBorderland sorts desc
    EXPECT_DOUBLE_EQ(pool.fitnesses[3], 0.5);

    // Re-running mutator on the SAME pool should bump the same genomes
    // a second time — i.e. mutateBorderlandPool is in-place, not a copy.
    const auto* g0 = dynamic_cast<const NeatGenome*>(pool.genomes[0].get());
    ASSERT_NE(g0, nullptr);
    ASSERT_FALSE(g0->connections().empty());
    const float w_after_one_round = g0->connections().front().weight;
    trio.mutateBorderlandPool(/*archetype=*/2, mutator);
    const float w_after_two_rounds = g0->connections().front().weight;
    EXPECT_FLOAT_EQ(w_after_two_rounds, w_after_one_round + 0.5f);
}

// Empty pool / null mutator must return 0 and not touch state.
TEST(Layer6Migration, MutateBorderlandPoolNoOpOnEmptyOrNullMutator) {
    TripleWorldRuntime trio;

    // Empty pool early-returns without growing the slot's eval counter.
    EXPECT_EQ(trio.mutateBorderlandPool(/*archetype=*/9,
                                        [](IGenome&) { /* unused */ }),
              0u);

    // Seed a pool, then call with null mutator.
    std::vector<std::unique_ptr<IGenome>> pop;
    pop.push_back(makeTinyGenome(GenomeId{1200u}));
    std::vector<double> fits = {1.0};
    std::vector<std::uint64_t> lin = {1210u};
    trio.promoteExternalToBorderland(/*archetype=*/9, pop, fits, lin, 1);
    EXPECT_EQ(trio.mutateBorderlandPool(/*archetype=*/9,
                                        TripleWorldRuntime::BorderlandMutateFn{}),
              0u);
}

// Empty pool / null eval_fn must NOT bump the counter — early return.
TEST(Layer6Migration, EvaluateBorderlandEarlyReturnDoesNotBumpEvalCount) {
    TripleWorldRuntime trio;
    // archetype 7 has nothing in borderland.
    EXPECT_EQ(trio.evaluateBorderland(/*archetype=*/7,
                                      [](const IGenome&) { return 1.0; },
                                      /*aging_factor=*/1.0),
              0u);
    // ensurePool will have created the (empty) slot, but the counter
    // must still be 0 because evaluateBorderland early-returned.
    EXPECT_EQ(trio.borderland().pools_by_archetype.at(7).borderland_eval_count, 0u);

    // Seed a pool, then call with a null eval_fn — also early return.
    std::vector<std::unique_ptr<IGenome>> pop;
    pop.push_back(makeTinyGenome(GenomeId{980u}));
    std::vector<double> fits = {4.0};
    std::vector<std::uint64_t> lin = {990u};
    trio.promoteExternalToBorderland(/*archetype=*/7, pop, fits, lin, 1);
    EXPECT_EQ(trio.evaluateBorderland(/*archetype=*/7,
                                      /*eval_fn=*/nullptr,
                                      /*aging_factor=*/1.0),
              0u);
    EXPECT_EQ(trio.borderland().pools_by_archetype.at(7).borderland_eval_count, 0u);
}
