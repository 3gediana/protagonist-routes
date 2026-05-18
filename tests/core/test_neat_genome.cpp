#include "core/brain/neat/InnovationTracker.h"
#include "core/brain/neat/NeatGenome.h"
#include "core/evolution/speciation/CompatibilityDistance.h"

#include <gtest/gtest.h>

#include <random>

using neuro::CompatibilityDistance;
using neuro::GenomeId;
using neuro::InnovationId;
using neuro::InnovationTracker;
using neuro::NeatGenome;

TEST(NeatGenome, MinimalGenomeCreatesInputOutputNodesAndConnections) {
    InnovationTracker tracker(1, 1);
    std::mt19937 rng(42);

    auto genome = NeatGenome::minimal(GenomeId{1}, 3, 2, tracker, rng);

    ASSERT_NE(genome, nullptr);
    EXPECT_EQ(genome->inputDim(), 3u);
    EXPECT_EQ(genome->outputDim(), 2u);
    EXPECT_EQ(genome->inputNodeIds().size(), 3u);
    EXPECT_EQ(genome->outputNodeIds().size(), 2u);
    EXPECT_EQ(genome->connectionCount(), 6u);
    EXPECT_EQ(genome->complexity(), genome->nodeCount() + genome->connectionCount());
    EXPECT_EQ(tracker.nextNodeId(), 6u);
}

TEST(NeatGenome, InnovationTrackerReusesSameInnovationWithinGeneration) {
    InnovationTracker tracker(1, 1);

    const InnovationId first = tracker.nextConnectionInnovation(1, 2);
    const InnovationId second = tracker.nextConnectionInnovation(1, 2);
    EXPECT_EQ(first, second);

    tracker.beginGeneration();
    const InnovationId third = tracker.nextConnectionInnovation(1, 2);
    EXPECT_NE(first, third);
}

TEST(NeatGenome, CompatibilityDistanceIsZeroForCloneAndPositiveForWeightDifference) {
    InnovationTracker tracker(1, 1);
    std::mt19937 rng(42);
    auto genome = NeatGenome::minimal(GenomeId{1}, 2, 1, tracker, rng);

    auto clone = genome->clone();
    auto* clone_genome = dynamic_cast<NeatGenome*>(clone.get());
    ASSERT_NE(clone_genome, nullptr);

    CompatibilityDistance distance(1.0, 1.0, 0.4);
    EXPECT_DOUBLE_EQ(distance.compute(*genome, *clone_genome), 0.0);

    clone_genome->connections().front().weight += 1.5f;
    EXPECT_GT(distance.compute(*genome, *clone_genome), 0.0);
}
