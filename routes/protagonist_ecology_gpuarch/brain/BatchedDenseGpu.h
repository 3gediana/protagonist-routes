#pragma once

#include <cstddef>
#include <memory>
#include <span>

namespace neuro::routes::protagonist {

// Batched dense forward for N agents that share the same per-layer dimensions
// (input_dim, hidden_dim, action_dim, interface_dim).
//
// Each agent has its own weights/biases, but the runner concatenates them into
// flat [N * stride] buffers so a single CUDA call fans the matmul out to all
// agents at once. This amortises CUDA launch / memcpy / sync overhead which
// dominates the per-agent ProtagonistDenseGpu path.
//
// All host buffers must already be packed in agent-major layout:
//   inputs[a * input_dim + i]
//   weights_in_hidden[a * input_dim * hidden_dim + i * hidden_dim + h]
//   etc.
class BatchedDenseGpu final {
public:
    BatchedDenseGpu();
    ~BatchedDenseGpu();

    BatchedDenseGpu(BatchedDenseGpu&& other) noexcept;
    BatchedDenseGpu& operator=(BatchedDenseGpu&& other) noexcept;
    BatchedDenseGpu(const BatchedDenseGpu&) = delete;
    BatchedDenseGpu& operator=(const BatchedDenseGpu&) = delete;

    static bool supported();
    bool ready() const;

    // --- cohort-resident weights (2026-06-01 GPU-arch refactor) ----------------
    // Genome weights/biases are constant over an individual's lifetime. Upload
    // them once per cohort with uploadCohort(), then call forwardResident() each
    // tick: only the small changing tensors (inputs / addend / alphas / plastic
    // state) cross PCIe, instead of re-uploading the full ~14 MB weight set every
    // tick as forward() does. forwardResident() returns false if no matching
    // cohort is resident (caller should fall back to forward()/forwardCpu()).
    bool uploadCohort(std::size_t batch_size,
                      std::size_t input_dim,
                      std::size_t hidden_dim,
                      std::size_t action_dim,
                      std::size_t interface_dim,
                      std::span<const float> weights_in_hidden,
                      std::span<const float> hidden_bias,
                      std::span<const float> weights_hidden_action,
                      std::span<const float> action_bias,
                      std::span<const float> weights_hidden_interface,
                      std::span<const float> interface_bias);

    bool forwardResident(std::size_t batch_size,
                         std::size_t input_dim,
                         std::size_t hidden_dim,
                         std::size_t action_dim,
                         std::size_t interface_dim,
                         std::span<const float> inputs,
                         std::span<const float> hidden_addend,
                         std::span<const float> hidden_alphas,
                         std::span<const float> action_alphas,
                         std::span<float> hidden_io,
                         std::span<float> outputs_io,
                         std::span<float> interface_out);

    bool forward(std::size_t batch_size,
                 std::size_t input_dim,
                 std::size_t hidden_dim,
                 std::size_t action_dim,
                 std::size_t interface_dim,
                 std::span<const float> inputs,
                 std::span<const float> weights_in_hidden,
                 std::span<const float> hidden_bias,
                 std::span<const float> weights_hidden_action,
                 std::span<const float> action_bias,
                 std::span<const float> weights_hidden_interface,
                 std::span<const float> interface_bias,
                 std::span<const float> hidden_addend,
                 std::span<const float> hidden_alphas,
                 std::span<const float> action_alphas,
                 std::span<float> hidden_io,
                 std::span<float> outputs_io,
                 std::span<float> interface_out);

    // Reference CPU implementation (also used as fallback when CUDA isn't
    // available). Numerics match the CUDA kernel within float rounding.
    static void forwardCpu(std::size_t batch_size,
                           std::size_t input_dim,
                           std::size_t hidden_dim,
                           std::size_t action_dim,
                           std::size_t interface_dim,
                           std::span<const float> inputs,
                           std::span<const float> weights_in_hidden,
                           std::span<const float> hidden_bias,
                           std::span<const float> weights_hidden_action,
                           std::span<const float> action_bias,
                           std::span<const float> weights_hidden_interface,
                           std::span<const float> interface_bias,
                           std::span<const float> hidden_addend,
                           std::span<const float> hidden_alphas,
                           std::span<const float> action_alphas,
                           std::span<float> hidden_io,
                           std::span<float> outputs_io,
                           std::span<float> interface_out);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace neuro::routes::protagonist
