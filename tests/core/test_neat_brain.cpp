#include "core/brain/neat/ConnectionGene.h"
#include "core/brain/neat/NeatBrain.h"
#include "core/brain/neat/NeatGenome.h"
#include "core/brain/neat/NodeGene.h"

#include <gtest/gtest.h>

#include <vector>

using neuro::ConnectionGene;
using neuro::GenomeId;
using neuro::InnovationId;
using neuro::NeatBrain;
using neuro::NeatGenome;
using neuro::NodeGene;
using neuro::NodeGeneType;

namespace {

NeatGenome makeRecurrentGenome() {
    std::vector<NodeGene> nodes{
        NodeGene{1, NodeGeneType::Input, 0.0f, 1.0f, 1.0f},
        NodeGene{2, NodeGeneType::Output, 0.0f, 1.0f, 1.0f},
    };
    std::vector<ConnectionGene> connections{
        ConnectionGene{1, 2, 1.0f, true, InnovationId{1}},
        ConnectionGene{2, 2, 1.0f, true, InnovationId{2}},
    };
    return NeatGenome(GenomeId{1}, 1, 1, std::move(nodes), std::move(connections));
}

}  // namespace

TEST(NeatBrain, ForwardProducesExpectedShape) {
    NeatGenome genome = makeRecurrentGenome();
    NeatBrain brain(genome);

    const auto outputs = brain.forward(std::vector<float>{1.0f}, 1.0f);
    ASSERT_EQ(outputs.size(), 1u);
}

TEST(NeatBrain, InputMismatchSilentDegrade) {
    // Layer 4 fix (commit bd07564): NeatBrain::forward used to throw on
    // dim mismatch, but parallelFor workers can't unwind out of throws,
    // so a single bad genome would crash the whole population mid-tick.
    // Now it logs an error and returns zeros so the rest of the pop
    // keeps running and the mismatch is visible in logs.
    NeatGenome genome = makeRecurrentGenome();
    NeatBrain brain(genome);

    auto outputs = brain.forward(std::vector<float>{1.0f, 2.0f}, 1.0f);
    EXPECT_EQ(outputs.size(), 1u);
    EXPECT_FLOAT_EQ(outputs[0], 0.0f);
}

TEST(NeatBrain, RecurrentStatePersistsUntilReset) {
    NeatGenome genome = makeRecurrentGenome();
    NeatBrain brain(genome);

    const float first = brain.forward(std::vector<float>{1.0f}, 1.0f)[0];
    const float second = brain.forward(std::vector<float>{0.0f}, 1.0f)[0];

    EXPECT_GT(first, 0.0f);
    EXPECT_GT(second, 0.0f);

    brain.reset();
    const float after_reset = brain.forward(std::vector<float>{0.0f}, 1.0f)[0];
    EXPECT_FLOAT_EQ(after_reset, 0.0f);
}
