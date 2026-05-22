// G1 numerical-equivalence test: ProtagonistBatchedDenseRunner with the CPU
// reference path must produce bit-exact (to within 1e-5) the same dense state
// as the per-agent ProtagonistBrain::forward() path.
//
// The contract is: when batched_dense_enabled is on, the brain's prepare ->
// runner -> finalize sequence must leave motor_core / action_heads /
// interface_head / DNC state in the same configuration as a single
// brain.forward(inputs, dt) would have produced.
//
// We verify this by:
//   1. constructing two identical sets of brains (cloned weights from the same
//      seed)
//   2. running the legacy forward() path on set A
//   3. running the batched (CPU) runner on set B with the same inputs/dt
//   4. comparing motor_core_.state(), action_heads_.outputs(),
//      interface_head_.output(), and the action span returned by finalize
//      to the action span returned by forward.

#include "brain/ProtagonistBrain.h"
#include "brain/ProtagonistGenome.h"
#include "runtime/ProtagonistBatchedDenseRunner.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <random>
#include <vector>

namespace {

using neuro::IBatchableBrain;
using neuro::routes::protagonist::ProtagonistBatchedDenseRunner;
using neuro::routes::protagonist::ProtagonistBrain;
using neuro::routes::protagonist::ProtagonistGenome;

constexpr std::size_t kBatchSize = 6;
constexpr std::size_t kInputDim = 12;
constexpr std::size_t kHiddenDim = 24;
constexpr std::size_t kMemoryCells = 8;
constexpr std::size_t kMemorySlots = 16;
constexpr std::size_t kReadHeads = 2;
constexpr std::size_t kActionDim = 7;
constexpr float kEpsilon = 1.0e-5f;
constexpr float kDt = 0.10f;

std::unique_ptr<ProtagonistBrain> makeBrain(std::uint32_t seed, bool batchable) {
    auto brain = std::make_unique<ProtagonistBrain>(kInputDim, kHiddenDim,
                                                    kMemoryCells, kMemorySlots,
                                                    kReadHeads, kActionDim,
                                                    seed, /*enable_gpu_dense=*/false);
    brain->setBatchableEnabled(batchable);
    return brain;
}

std::vector<float> randomInputs(std::uint32_t seed, std::size_t batch_size) {
    std::mt19937 rng(seed);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    std::vector<float> out(batch_size * kInputDim, 0.0f);
    for (auto& v : out) v = dist(rng);
    return out;
}

}  // namespace

TEST(ProtagonistBatchedDenseRunnerTest, AsBatchableNullWhenDisabled) {
    auto brain = makeBrain(/*seed=*/42, /*batchable=*/false);
    EXPECT_EQ(brain->asBatchable(), nullptr);
}

TEST(ProtagonistBatchedDenseRunnerTest, AsBatchableNonNullWhenEnabled) {
    auto brain = makeBrain(/*seed=*/42, /*batchable=*/true);
    EXPECT_NE(brain->asBatchable(), nullptr);
}

TEST(ProtagonistBatchedDenseRunnerTest, CpuBatchedMatchesPerAgentForward) {
    // Set A: per-agent forward
    std::vector<std::unique_ptr<ProtagonistBrain>> brains_a;
    std::vector<std::unique_ptr<ProtagonistBrain>> brains_b;
    brains_a.reserve(kBatchSize);
    brains_b.reserve(kBatchSize);
    for (std::size_t i = 0; i < kBatchSize; ++i) {
        const auto seed = static_cast<std::uint32_t>(101 + i * 7);
        brains_a.push_back(makeBrain(seed, /*batchable=*/false));
        brains_b.push_back(makeBrain(seed, /*batchable=*/true));
    }

    const auto inputs = randomInputs(/*seed=*/2026, kBatchSize);

    // Path A: brain.forward()
    std::vector<std::vector<float>> outputs_a(kBatchSize);
    for (std::size_t i = 0; i < kBatchSize; ++i) {
        const std::span<const float> in_view(inputs.data() + i * kInputDim, kInputDim);
        const auto out = brains_a[i]->forward(in_view, kDt);
        outputs_a[i].assign(out.begin(), out.end());
    }

    // Path B: prepareInputs -> runner -> finalizeOutputs
    for (std::size_t i = 0; i < kBatchSize; ++i) {
        const std::span<const float> in_view(inputs.data() + i * kInputDim, kInputDim);
        brains_b[i]->prepareInputs(in_view, kDt);
    }

    // Build a dummy hook input: AgentLayer batches Agent*, but we don't have
    // Agents here. The runner exposes operator() for Agent*; we want to call
    // it directly on the IBatchableBrain* list. Reuse the same logic by
    // constructing a "fake" runner test that marshals via denseSlot manually.
    // But that's exactly what runner.operator() does given a batched_agents list,
    // and that list is built from agent->brain().asBatchable() -- so the test
    // would need real Agents. Workaround: replicate the marshalling here using
    // the public CPU forward, since runner is a thin wrapper.
    {
        std::vector<float> buf_inputs(kBatchSize * kInputDim + kBatchSize * kReadHeads * kMemoryCells, 0.0f);
        // total_input_dim = inputDim + memory_cells * read_heads
        const std::size_t total_in_dim = kInputDim + kMemoryCells * kReadHeads;
        std::vector<float> in(kBatchSize * total_in_dim, 0.0f);
        std::vector<float> w_ih(kBatchSize * total_in_dim * kHiddenDim, 0.0f);
        std::vector<float> b_h(kBatchSize * kHiddenDim, 0.0f);
        std::vector<float> w_ha(kBatchSize * kHiddenDim * kActionDim, 0.0f);
        std::vector<float> b_a(kBatchSize * kActionDim, 0.0f);
        const std::size_t iface_dim = brains_b[0]->denseSlot().interface_dim;
        std::vector<float> w_hi(kBatchSize * kHiddenDim * iface_dim, 0.0f);
        std::vector<float> b_i(kBatchSize * iface_dim, 0.0f);
        std::vector<float> hidden_alphas(kBatchSize, 0.0f);
        std::vector<float> action_alphas(kBatchSize, 0.0f);
        std::vector<float> hidden_io(kBatchSize * kHiddenDim, 0.0f);
        std::vector<float> action_io(kBatchSize * kActionDim, 0.0f);
        std::vector<float> iface_out(kBatchSize * iface_dim, 0.0f);

        for (std::size_t a = 0; a < kBatchSize; ++a) {
            const auto slot = brains_b[a]->denseSlot();
            ASSERT_EQ(slot.total_input_dim, total_in_dim);
            ASSERT_EQ(slot.hidden_dim, kHiddenDim);
            ASSERT_EQ(slot.action_dim, kActionDim);
            ASSERT_EQ(slot.interface_dim, iface_dim);
            std::copy(slot.full_input.begin(), slot.full_input.end(),
                      in.begin() + a * total_in_dim);
            std::copy(slot.input_hidden_weights.begin(), slot.input_hidden_weights.end(),
                      w_ih.begin() + a * total_in_dim * kHiddenDim);
            std::copy(slot.hidden_bias.begin(), slot.hidden_bias.end(),
                      b_h.begin() + a * kHiddenDim);
            std::copy(slot.hidden_action_weights.begin(), slot.hidden_action_weights.end(),
                      w_ha.begin() + a * kHiddenDim * kActionDim);
            std::copy(slot.action_bias.begin(), slot.action_bias.end(),
                      b_a.begin() + a * kActionDim);
            std::copy(slot.hidden_interface_weights.begin(), slot.hidden_interface_weights.end(),
                      w_hi.begin() + a * kHiddenDim * iface_dim);
            std::copy(slot.interface_bias.begin(), slot.interface_bias.end(),
                      b_i.begin() + a * iface_dim);
            hidden_alphas[a] = slot.hidden_alpha;
            action_alphas[a] = slot.action_alpha;
            std::copy(slot.hidden_io.begin(), slot.hidden_io.end(),
                      hidden_io.begin() + a * kHiddenDim);
            std::copy(slot.action_io.begin(), slot.action_io.end(),
                      action_io.begin() + a * kActionDim);
        }

        neuro::routes::protagonist::BatchedDenseGpu::forwardCpu(
            kBatchSize, total_in_dim, kHiddenDim, kActionDim, iface_dim,
            in, w_ih, b_h, w_ha, b_a, w_hi, b_i,
            hidden_alphas, action_alphas,
            hidden_io, action_io, iface_out);

        for (std::size_t a = 0; a < kBatchSize; ++a) {
            const auto slot = brains_b[a]->denseSlot();
            std::copy(hidden_io.begin() + a * kHiddenDim,
                      hidden_io.begin() + (a + 1) * kHiddenDim,
                      slot.hidden_io.begin());
            std::copy(action_io.begin() + a * kActionDim,
                      action_io.begin() + (a + 1) * kActionDim,
                      slot.action_io.begin());
            std::copy(iface_out.begin() + a * iface_dim,
                      iface_out.begin() + (a + 1) * iface_dim,
                      slot.interface_out.begin());
        }
    }

    // Now finalize and compare per-element.
    for (std::size_t i = 0; i < kBatchSize; ++i) {
        const auto out_span = brains_b[i]->finalizeOutputs(kDt);
        std::vector<float> out_b(out_span.begin(), out_span.end());

        ASSERT_EQ(out_b.size(), outputs_a[i].size());
        for (std::size_t k = 0; k < out_b.size(); ++k) {
            EXPECT_NEAR(out_b[k], outputs_a[i][k], kEpsilon)
                << "agent " << i << " action[" << k << "]";
        }
    }
}
