#include "brain/ProtagonistBrain.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <vector>

TEST(ProtagonistBrain, ExposesMinimalBrainV3ShellState) {
    neuro::routes::protagonist::ProtagonistBrain brain(
        10,
        8,
        4,
        6,
        2,
        3,
        123u,
        false);

    const std::vector<float> inputs(10, 0.25f);
    const auto outputs = brain.forward(inputs, 0.1f);

    EXPECT_EQ(outputs.size(), 3u);
    EXPECT_GT(brain.workingMemory().totalCellCount(), 0u);
    EXPECT_GT(brain.bodyStateMemory().summaryDim(), 0u);
}

TEST(ProtagonistBrain, FeedsBodyAndWorkingMemoryIntoLearnablePath) {
    neuro::routes::protagonist::ProtagonistBrain baseline(
        10,
        8,
        4,
        6,
        2,
        3,
        123u,
        false);
    neuro::routes::protagonist::ProtagonistBrain primed(
        10,
        8,
        4,
        6,
        2,
        3,
        123u,
        false);

    const std::vector<float> body_observation = {0.8f, -0.6f, 0.4f, -0.2f, 0.9f, -0.7f, 0.5f, -0.3f, 0.2f, -0.1f};
    const std::vector<float> fused_hidden = {0.7f, -0.5f, 0.4f, -0.2f, 0.6f, -0.3f, 0.2f, -0.1f};
    primed.bodyStateMemory().observe(body_observation, 1.0f);
    primed.workingMemory().observe(fused_hidden, 1.0f);

    const std::vector<float> inputs(10, 0.0f);
    const auto baseline_outputs_span = baseline.forward(inputs, 0.1f);
    const auto primed_outputs_span = primed.forward(inputs, 0.1f);
    const std::vector<float> baseline_outputs(baseline_outputs_span.begin(), baseline_outputs_span.end());
    const std::vector<float> primed_outputs(primed_outputs_span.begin(), primed_outputs_span.end());

    float max_diff = 0.0f;
    for (std::size_t i = 0; i < baseline_outputs.size(); ++i) {
        max_diff = std::max(max_diff, std::fabs(baseline_outputs[i] - primed_outputs[i]));
    }

    EXPECT_GT(max_diff, 1.0e-5f);
}

TEST(ProtagonistBrain, FeedsWorkingMemoryIntoLearnablePath) {
    neuro::routes::protagonist::ProtagonistBrain baseline(
        10,
        8,
        4,
        6,
        2,
        3,
        123u,
        false);
    neuro::routes::protagonist::ProtagonistBrain primed(
        10,
        8,
        4,
        6,
        2,
        3,
        123u,
        false);

    const std::vector<float> fused_hidden = {0.7f, -0.5f, 0.4f, -0.2f, 0.6f, -0.3f, 0.2f, -0.1f};
    primed.workingMemory().observe(fused_hidden, 1.0f);

    const std::vector<float> inputs(10, 0.0f);
    const auto baseline_outputs_span = baseline.forward(inputs, 0.1f);
    const auto primed_outputs_span = primed.forward(inputs, 0.1f);
    const std::vector<float> baseline_outputs(baseline_outputs_span.begin(), baseline_outputs_span.end());
    const std::vector<float> primed_outputs(primed_outputs_span.begin(), primed_outputs_span.end());

    float max_diff = 0.0f;
    for (std::size_t i = 0; i < baseline_outputs.size(); ++i) {
        max_diff = std::max(max_diff, std::fabs(baseline_outputs[i] - primed_outputs[i]));
    }

    EXPECT_GT(max_diff, 1.0e-5f);
}
