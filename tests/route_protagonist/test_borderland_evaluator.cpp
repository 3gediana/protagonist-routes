// Layer 6 R3 stage 3 commit D (skeleton) tests for BorderlandEvaluator.
// Centralizes the eval_fn in a single class so commit E can swap the
// implementation without touching ProtagonistEcologyRuntime.

#include "core/brain/neat/NeatGenome.h"
#include "runtime/BorderlandEvaluator.h"
#include "runtime/MigrationScheduler.h"
#include "runtime/ProtagonistEcologyConfig.h"
#include "runtime/TripleWorldRuntime.h"

#include <gtest/gtest.h>

#include <memory>
#include <vector>

namespace {

using neuro::ConnectionGene;
using neuro::GenomeId;
using neuro::IGenome;
using neuro::NeatGenome;
using neuro::NodeGene;
using neuro::NodeGeneType;
using neuro::NodeId;
using neuro::routes::protagonist::BorderlandEvaluator;
using neuro::routes::protagonist::MigrationCandidate;
using neuro::routes::protagonist::MigrationScheduler;
using neuro::routes::protagonist::ProtagonistEcologyConfig;
using neuro::routes::protagonist::TripleWorldRuntime;

std::unique_ptr<IGenome> makeGenome(GenomeId id, std::size_t extra_nodes) {
    std::vector<NodeGene> nodes;
    nodes.push_back(NodeGene{static_cast<NodeId>(1), NodeGeneType::Input,  0.0f, 1.0f, 1.0f});
    nodes.push_back(NodeGene{static_cast<NodeId>(2), NodeGeneType::Output, 0.0f, 1.0f, 1.0f});
    // Pad with extra hidden nodes so complexity differs per genome.
    for (std::size_t i = 0; i < extra_nodes; ++i) {
        nodes.push_back(NodeGene{static_cast<NodeId>(3 + i),
                                 NodeGeneType::Hidden, 0.0f, 1.0f, 1.0f});
    }
    std::vector<ConnectionGene> conns;
    conns.push_back(ConnectionGene{static_cast<NodeId>(1), static_cast<NodeId>(2),
                                   0.1f, true, neuro::kInvalidInnovationId});
    return std::make_unique<NeatGenome>(id, /*input=*/1, /*output=*/1,
                                        std::move(nodes), std::move(conns), 1.0f);
}

}  // namespace

// Placeholder evaluation: each genome's stashed fitness is replaced
// with max(aged_old, complexity * 100). Aging factor 1.0 means the
// fresh score wins whenever it exceeds the old.
TEST(Layer6BorderlandEvaluator, PlaceholderFitnessUpdatesPool) {
    TripleWorldRuntime trio;
    std::vector<std::unique_ptr<IGenome>> pop;
    // Three genomes with complexities 3 (2 base nodes + 1 hidden + 1 conn = 4),
    // 5 (2+2 nodes + 1 conn = 5), 7 (2+4 nodes + 1 conn = 7). NeatGenome::
    // complexity returns nodes.size() + connections.size().
    pop.push_back(makeGenome(GenomeId{401u}, /*extra_nodes=*/1));  // complexity 4
    pop.push_back(makeGenome(GenomeId{402u}, /*extra_nodes=*/3));  // complexity 6
    pop.push_back(makeGenome(GenomeId{403u}, /*extra_nodes=*/5));  // complexity 8
    std::vector<double> fits = {0.0, 0.0, 0.0};
    std::vector<std::uint64_t> lineages = {410u, 411u, 412u};
    trio.promoteExternalToBorderland(/*archetype=*/4, pop, fits, lineages, 3);

    ProtagonistEcologyConfig route_stub{};
    const std::size_t evaluated = BorderlandEvaluator::evaluatePool(
        trio, /*archetype=*/4, route_stub, /*aging_factor=*/1.0);
    EXPECT_EQ(evaluated, 3u);

    const auto& pool = trio.borderland().pools_by_archetype.at(4);
    ASSERT_EQ(pool.fitnesses.size(), 3u);
    // Each fitness must be > 0 (placeholder = complexity * 100 >= 400)
    // and proportional to per-genome complexity. Since
    // promoteExternalToBorderland stable-sorts descending by source
    // fitness (all zero here, so order is preserved), the pool stores
    // genomes in the order [401, 402, 403] with complexities [4, 6, 8].
    EXPECT_DOUBLE_EQ(pool.fitnesses[0], 400.0);
    EXPECT_DOUBLE_EQ(pool.fitnesses[1], 600.0);
    EXPECT_DOUBLE_EQ(pool.fitnesses[2], 800.0);

    // The eval count counter on the pool was bumped (commit C contract).
    EXPECT_EQ(pool.borderland_eval_count, 1u);
}

// Empty pool / unknown archetype: evaluatePool must return 0 and not
// touch any state.
TEST(Layer6BorderlandEvaluator, EmptyPoolReturnsZero) {
    TripleWorldRuntime trio;
    ProtagonistEcologyConfig route_stub{};
    EXPECT_EQ(BorderlandEvaluator::evaluatePool(trio, /*archetype=*/9, route_stub, 1.0), 0u);
    EXPECT_EQ(trio.borderland().pools_by_archetype.at(9).borderland_eval_count, 0u);
}
