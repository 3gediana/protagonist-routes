// Stage 5 / P5 MAP-Elites archive smoke test.

#include "core/evolution/novelty/MapElitesArchive.h"

#include <gtest/gtest.h>

namespace {
using neuro::BehaviorCharacterization;
using neuro::GenomeId;
using neuro::MapElitesArchive;
}

TEST(MapElitesArchive, NewBehaviorOpensNewCellAndElitesAreFitnessReplaced) {
    MapElitesArchive arch(/*bins_per_dim*/ 4, /*value_min*/ 0.0f, /*value_max*/ 1.0f);

    BehaviorCharacterization a{0.10f, 0.10f, 0.20f, 0.30f, 0.40f, 0.50f, 0.60f, 0.70f};
    BehaviorCharacterization b{0.80f, 0.80f, 0.50f, 0.30f, 0.40f, 0.50f, 0.60f, 0.70f};
    BehaviorCharacterization c{0.15f, 0.15f, 0.20f, 0.30f, 0.40f, 0.50f, 0.60f, 0.70f};  // same cell as a

    EXPECT_TRUE(arch.considerCandidate(GenomeId{1}, a, /*fit*/ 5.0f));
    EXPECT_EQ(arch.cellCount(), 1u);

    EXPECT_TRUE(arch.considerCandidate(GenomeId{2}, b, /*fit*/ 10.0f));
    EXPECT_EQ(arch.cellCount(), 2u);

    // Submitting a worse score in cell-a's bucket must not displace it.
    EXPECT_FALSE(arch.considerCandidate(GenomeId{3}, c, /*fit*/ 2.0f));
    EXPECT_EQ(arch.cellCount(), 2u);

    // Submitting a better score in the same bucket must.
    EXPECT_TRUE(arch.considerCandidate(GenomeId{4}, c, /*fit*/ 7.0f));
    EXPECT_EQ(arch.cellCount(), 2u);

    EXPECT_FLOAT_EQ(arch.bestFitness(), 10.0f);
    EXPECT_NEAR(arch.avgFitness(), (7.0f + 10.0f) * 0.5f, 1e-5f);
}
