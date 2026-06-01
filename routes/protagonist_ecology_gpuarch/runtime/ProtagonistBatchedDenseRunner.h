#pragma once

#include "brain/BatchedDenseGpu.h"

#include <cstddef>
#include <vector>

namespace neuro {
class Agent;
class IWorld;
}  // namespace neuro

namespace neuro::routes::protagonist {

// Batched decision hook for AgentLayer. Collects all agents whose brain
// implements IBatchableBrain and folds their dense matmul into a single
// CUDA call (or CPU reference path when CUDA is unavailable).
//
// All batched agents must share the same per-layer dimensions
// (input_dim, hidden_dim, action_dim, interface_dim). If any agent has a
// mismatched dim it is skipped (its brain falls back to per-agent forward()
// in decideAfterDense() automatically).
class ProtagonistBatchedDenseRunner final {
public:
    enum class Backend {
        kAuto,   // GPU if available, CPU otherwise
        kCpu,    // force CPU fallback (used by tests)
        kGpu,    // force GPU (returns false from operator() if not ready)
    };

    explicit ProtagonistBatchedDenseRunner(Backend backend = Backend::kAuto);
    ~ProtagonistBatchedDenseRunner();

    ProtagonistBatchedDenseRunner(const ProtagonistBatchedDenseRunner&) = delete;
    ProtagonistBatchedDenseRunner& operator=(const ProtagonistBatchedDenseRunner&) = delete;
    ProtagonistBatchedDenseRunner(ProtagonistBatchedDenseRunner&&) noexcept;
    ProtagonistBatchedDenseRunner& operator=(ProtagonistBatchedDenseRunner&&) noexcept;

    bool gpuReady() const;
    Backend activeBackend() const { return active_backend_; }

    // Hook entry. Signature matches BatchedDecisionHook in AgentLayer.h.
    void operator()(IWorld& world, float dt, const std::vector<Agent*>& batched_agents);

    // For tests/diagnostics: number of agents in the last batch.
    std::size_t lastBatchSize() const { return last_batch_size_; }
    bool lastBatchUsedGpu() const { return last_batch_used_gpu_; }
    // True iff the last tick reused cohort-resident weights (no weight re-upload).
    bool lastBatchReusedWeights() const { return last_batch_reused_weights_; }

private:
    Backend backend_choice_;
    Backend active_backend_;
    BatchedDenseGpu gpu_;
    std::size_t last_batch_size_{0};
    bool last_batch_used_gpu_{false};
    bool last_batch_reused_weights_{false};

    // Cohort identity for resident-weights reuse. Genome weights are constant
    // over a lifetime, so we only re-marshal + re-upload the weight matrices
    // when the cohort changes (new generation, or membership changes mid-gen
    // when an agent dies/spawns). last_cohort_ptrs_ + last_fingerprint_ detect
    // that cheaply (O(batch), samples a few weights per agent) without hashing
    // the full weight set every tick.
    std::vector<const void*> last_cohort_ptrs_;
    unsigned long long last_fingerprint_{0};
    bool cohort_resident_{false};

    // Marshalling buffers (reused across calls to avoid reallocation).
    std::vector<float> buf_inputs_;
    std::vector<float> buf_weights_ih_;
    std::vector<float> buf_bias_h_;
    std::vector<float> buf_weights_ha_;
    std::vector<float> buf_bias_a_;
    std::vector<float> buf_weights_hi_;
    std::vector<float> buf_bias_i_;
    std::vector<float> buf_hidden_addend_;
    std::vector<float> buf_hidden_alphas_;
    std::vector<float> buf_action_alphas_;
    std::vector<float> buf_hidden_io_;
    std::vector<float> buf_action_io_;
    std::vector<float> buf_interface_out_;
};

}  // namespace neuro::routes::protagonist
