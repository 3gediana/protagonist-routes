#include "core/brain/fixed/FixedTopologyBrain.h"
#include "core/brain/fixed/FixedTopologyGenome.h"

#include <gtest/gtest.h>

#include <array>

using neuro::FixedTopologyBrain;
using neuro::FixedTopologyGenome;
using neuro::GenomeId;

TEST(FixedTopologyBrain, ForwardProducesExpectedShape) {
    FixedTopologyGenome genome(GenomeId{1}, 2, 2, 1);
    genome.inputHiddenWeights() = {
        1.0f, 0.0f,
        0.0f, 1.0f,
    };
    genome.hiddenBias() = {0.0f, 0.0f};
    genome.hiddenOutputWeights() = {1.0f, 1.0f};
    genome.outputBias() = {0.0f};

    FixedTopologyBrain brain(genome);
    const std::array<float, 2> inputs{0.5f, -0.5f};
    const auto output = brain.forward(inputs, 1.0f);

    ASSERT_EQ(output.size(), 1u);
    EXPECT_GT(output[0], -1.0f);
    EXPECT_LT(output[0], 1.0f);
}

TEST(FixedTopologyBrain, InputMismatchThrows) {
    FixedTopologyGenome genome(GenomeId{1}, 3, 2, 1);
    FixedTopologyBrain brain(genome);
    const std::array<float, 2> inputs{1.0f, 2.0f};
    EXPECT_THROW(brain.forward(inputs, 1.0f), std::invalid_argument);
}

TEST(FixedTopologyGenome, ComplexityEqualsParameterCount) {
    FixedTopologyGenome genome(GenomeId{1}, 5, 10, 2);
    EXPECT_EQ(genome.complexity(), genome.parameterCount());
}
