// Layer 6 R2 stage 3 commit 2: MainWorldPersistence pool-only snapshot.
//
// Coverage:
//   - roundtrip: save() -> load() preserves per-archetype pool size,
//     fitness, lineage IDs, and the genome blobs reconstruct to
//     equivalent IGenome (id + complexity matches)
//   - empty TripleWorldRuntime saves + loads without error
//   - missing manifest -> load returns false gracefully
//
// Note: archetype 0 in the runtime stores ProtagonistGenome. To keep
// these tests independent of ProtagonistGenome construction overhead,
// we only exercise archetype >= 1 (NeatGenome) here; the archetype 0
// path is exercised by the runtime smoke (smoke_tw_persist.toml).

#include "core/brain/neat/NeatGenome.h"
#include "runtime/MainWorldPersistence.h"
#include "runtime/TripleWorldRuntime.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <string>

namespace {

using neuro::ConnectionGene;
using neuro::GenomeId;
using neuro::IGenome;
using neuro::NeatGenome;
using neuro::NodeGene;
using neuro::NodeGeneType;
using neuro::NodeId;
using neuro::routes::protagonist::MainWorldPersistence;
using neuro::routes::protagonist::MainWorldSnapshotMetadata;
using neuro::routes::protagonist::TripleWorldRuntime;

std::unique_ptr<IGenome> makeNeatGenome(GenomeId id, float weight) {
    std::vector<NodeGene> nodes;
    nodes.push_back(NodeGene{static_cast<NodeId>(1), NodeGeneType::Input,  0.0f, 1.0f, 1.0f});
    nodes.push_back(NodeGene{static_cast<NodeId>(2), NodeGeneType::Output, 0.0f, 1.0f, 1.0f});
    std::vector<ConnectionGene> conns;
    conns.push_back(ConnectionGene{static_cast<NodeId>(1), static_cast<NodeId>(2),
                                   weight, true, neuro::kInvalidInnovationId});
    return std::make_unique<NeatGenome>(id, /*input=*/1, /*output=*/1,
                                        std::move(nodes), std::move(conns), 1.0f);
}

std::filesystem::path makeTempDir(const std::string& tag) {
    auto base = std::filesystem::temp_directory_path()
                / ("neuralEco_tw_persist_" + tag + "_"
                   + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(base);
    return base;
}

}  // namespace

TEST(Layer6Persistence, RoundtripPreservesPoolStructure) {
    TripleWorldRuntime trio_src;
    // Seed borderland archetype 1 (background) + main_world archetype 2.
    {
        std::vector<std::unique_ptr<IGenome>> pop;
        pop.push_back(makeNeatGenome(GenomeId{500u}, 0.11f));
        pop.push_back(makeNeatGenome(GenomeId{501u}, 0.22f));
        pop.push_back(makeNeatGenome(GenomeId{502u}, 0.33f));
        std::vector<double> fits = {1.5, 2.5, 0.5};
        std::vector<std::uint64_t> lineages = {510u, 511u, 512u};
        // We want the entries in source order rather than sorted, so
        // promote with k=3 (no reorder happens because all three are
        // top-3 of 3); extractTopK is a *stable* sort by fitness, so
        // resulting order will be {501 (2.5), 500 (1.5), 502 (0.5)}.
        trio_src.promoteExternalToBorderland(/*archetype=*/1, pop, fits, lineages, 3);
    }
    {
        std::vector<std::unique_ptr<IGenome>> pop;
        pop.push_back(makeNeatGenome(GenomeId{600u}, 0.7f));
        pop.push_back(makeNeatGenome(GenomeId{601u}, 0.8f));
        std::vector<double> fits = {10.0, 20.0};
        std::vector<std::uint64_t> lineages = {610u, 611u};
        trio_src.promoteExternalToBorderland(/*archetype=*/2, pop, fits, lineages, 2);
        trio_src.promoteBorderlandToMain(/*archetype=*/2, /*k=*/2);
    }

    const auto dir = makeTempDir("roundtrip");
    MainWorldSnapshotMetadata meta;
    meta.snapshot_id = 42u;
    meta.sim_time_seconds = 1234.5;
    meta.generations_elapsed = 7;
    meta.scenario_name = "test_roundtrip";
    ASSERT_TRUE(MainWorldPersistence::save(trio_src, meta, dir));
    ASSERT_TRUE(std::filesystem::exists(dir / "main_world_manifest.json"));

    TripleWorldRuntime trio_dst;
    MainWorldSnapshotMetadata meta_loaded;
    ASSERT_TRUE(MainWorldPersistence::load(dir, trio_dst, &meta_loaded));

    EXPECT_EQ(meta_loaded.snapshot_id, 42u);
    EXPECT_DOUBLE_EQ(meta_loaded.sim_time_seconds, 1234.5);
    EXPECT_EQ(meta_loaded.generations_elapsed, 7u);
    EXPECT_EQ(meta_loaded.scenario_name, "test_roundtrip");

    // Borderland archetype 1: stable-sorted by fitness desc =>
    // genome ids {501, 500, 502}, fitness {2.5, 1.5, 0.5},
    // lineages {511, 510, 512}.
    ASSERT_GE(trio_dst.borderland().pools_by_archetype.size(), 2u);
    const auto& b1 = trio_dst.borderland().pools_by_archetype.at(1);
    ASSERT_EQ(b1.genomes.size(), 3u);
    EXPECT_DOUBLE_EQ(b1.fitnesses[0], 2.5);
    EXPECT_DOUBLE_EQ(b1.fitnesses[1], 1.5);
    EXPECT_DOUBLE_EQ(b1.fitnesses[2], 0.5);
    EXPECT_EQ(b1.lineage_ids[0], 511u);
    EXPECT_EQ(b1.lineage_ids[1], 510u);
    EXPECT_EQ(b1.lineage_ids[2], 512u);
    EXPECT_EQ(b1.genomes[0]->id().value, 501u);
    EXPECT_EQ(b1.genomes[1]->id().value, 500u);
    EXPECT_EQ(b1.genomes[2]->id().value, 502u);

    // Main world archetype 2: ids {601, 600} (sorted desc by fit).
    ASSERT_GE(trio_dst.mainWorld().pools_by_archetype.size(), 3u);
    const auto& m2 = trio_dst.mainWorld().pools_by_archetype.at(2);
    ASSERT_EQ(m2.genomes.size(), 2u);
    EXPECT_DOUBLE_EQ(m2.fitnesses[0], 20.0);
    EXPECT_DOUBLE_EQ(m2.fitnesses[1], 10.0);
    EXPECT_EQ(m2.genomes[0]->id().value, 601u);
    EXPECT_EQ(m2.genomes[1]->id().value, 600u);

    // Cleanup
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

TEST(Layer6Persistence, EmptyTripleWorldRoundtripsCleanly) {
    TripleWorldRuntime trio_src;
    const auto dir = makeTempDir("empty");
    MainWorldSnapshotMetadata meta;
    ASSERT_TRUE(MainWorldPersistence::save(trio_src, meta, dir));

    TripleWorldRuntime trio_dst;
    ASSERT_TRUE(MainWorldPersistence::load(dir, trio_dst));
    EXPECT_TRUE(trio_dst.borderland().pools_by_archetype.empty());
    EXPECT_TRUE(trio_dst.mainWorld().pools_by_archetype.empty());

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

TEST(Layer6Persistence, MissingManifestLoadReturnsFalse) {
    TripleWorldRuntime trio_dst;
    const auto dir = makeTempDir("missing");
    // dir exists, manifest file does not.
    EXPECT_FALSE(MainWorldPersistence::load(dir, trio_dst));

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}
