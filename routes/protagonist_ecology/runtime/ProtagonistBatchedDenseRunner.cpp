#include "runtime/ProtagonistBatchedDenseRunner.h"

#include "core/agent/Agent.h"
#include "core/interfaces/IBrain.h"
#include "runtime/TrainingProfiler.h"

namespace neuro::routes::protagonist {

namespace {

// Returns IBatchableBrain* iff the brain advertises batched support; else nullptr.
IBatchableBrain* batchableOf(Agent* agent) {
    if (agent == nullptr) return nullptr;
    return agent->brain().asBatchable();
}

}  // namespace

ProtagonistBatchedDenseRunner::ProtagonistBatchedDenseRunner(Backend backend)
    : backend_choice_(backend), active_backend_(Backend::kCpu), gpu_() {
    if (backend == Backend::kCpu) {
        active_backend_ = Backend::kCpu;
    } else {
        if (gpu_.ready()) {
            active_backend_ = Backend::kGpu;
        } else if (backend == Backend::kGpu) {
            active_backend_ = Backend::kGpu;  // forced GPU but not ready; ops will fail until ready
        } else {
            active_backend_ = Backend::kCpu;
        }
    }
}

ProtagonistBatchedDenseRunner::~ProtagonistBatchedDenseRunner() = default;
ProtagonistBatchedDenseRunner::ProtagonistBatchedDenseRunner(ProtagonistBatchedDenseRunner&&) noexcept = default;
ProtagonistBatchedDenseRunner& ProtagonistBatchedDenseRunner::operator=(ProtagonistBatchedDenseRunner&&) noexcept = default;

bool ProtagonistBatchedDenseRunner::gpuReady() const {
    return gpu_.ready();
}

void ProtagonistBatchedDenseRunner::operator()(IWorld& world,
                                               float dt,
                                               const std::vector<Agent*>& batched_agents) {
    TrainingProfiler::Scope profile_scope(TrainingProfiler::Phase::kBrainForward);
    (void)world;
    (void)dt;
    last_batch_size_ = 0;
    last_batch_used_gpu_ = false;

    if (batched_agents.empty()) return;

    // First pass: discover the dim of the first batchable agent and filter
    // out any agents with mismatched dims.
    std::vector<IBatchableBrain*> brains;
    brains.reserve(batched_agents.size());
    BatchedDenseSlot probe{};
    bool have_probe = false;

    for (auto* agent : batched_agents) {
        auto* batchable = batchableOf(agent);
        if (batchable == nullptr) continue;
        const auto slot = batchable->denseSlot();
        if (!have_probe) {
            probe = slot;
            have_probe = true;
            brains.push_back(batchable);
            continue;
        }
        if (slot.total_input_dim != probe.total_input_dim
            || slot.hidden_dim != probe.hidden_dim
            || slot.action_dim != probe.action_dim
            || slot.interface_dim != probe.interface_dim) {
            continue;  // skip; brain falls back to per-agent forward inline
        }
        brains.push_back(batchable);
    }

    if (brains.empty()) return;

    const std::size_t batch = brains.size();
    const std::size_t in_dim = probe.total_input_dim;
    const std::size_t h_dim = probe.hidden_dim;
    const std::size_t a_dim = probe.action_dim;
    const std::size_t i_dim = probe.interface_dim;

    buf_inputs_.assign(batch * in_dim, 0.0f);
    buf_weights_ih_.assign(batch * in_dim * h_dim, 0.0f);
    buf_bias_h_.assign(batch * h_dim, 0.0f);
    buf_weights_ha_.assign(batch * h_dim * a_dim, 0.0f);
    buf_bias_a_.assign(batch * a_dim, 0.0f);
    buf_weights_hi_.assign(batch * h_dim * i_dim, 0.0f);
    buf_bias_i_.assign(batch * i_dim, 0.0f);
    buf_hidden_addend_.assign(batch * h_dim, 0.0f);
    buf_hidden_alphas_.assign(batch, 0.0f);
    buf_action_alphas_.assign(batch, 0.0f);
    buf_hidden_io_.assign(batch * h_dim, 0.0f);
    buf_action_io_.assign(batch * a_dim, 0.0f);
    buf_interface_out_.assign(batch * i_dim, 0.0f);

    // Marshal each agent's slot into agent-major flat buffers.
    for (std::size_t a = 0; a < batch; ++a) {
        const auto slot = brains[a]->denseSlot();

        std::copy(slot.full_input.begin(), slot.full_input.end(),
                  buf_inputs_.begin() + a * in_dim);
        std::copy(slot.input_hidden_weights.begin(), slot.input_hidden_weights.end(),
                  buf_weights_ih_.begin() + a * in_dim * h_dim);
        std::copy(slot.hidden_bias.begin(), slot.hidden_bias.end(),
                  buf_bias_h_.begin() + a * h_dim);
        std::copy(slot.hidden_action_weights.begin(), slot.hidden_action_weights.end(),
                  buf_weights_ha_.begin() + a * h_dim * a_dim);
        std::copy(slot.action_bias.begin(), slot.action_bias.end(),
                  buf_bias_a_.begin() + a * a_dim);
        std::copy(slot.hidden_interface_weights.begin(), slot.hidden_interface_weights.end(),
                  buf_weights_hi_.begin() + a * h_dim * i_dim);
        std::copy(slot.interface_bias.begin(), slot.interface_bias.end(),
                  buf_bias_i_.begin() + a * i_dim);
        // Working-memory injection addend (size h_dim per agent). Empty span
        // means the brain wants no addend; we leave the zero-filled buffer.
        if (!slot.hidden_target_addend.empty()) {
            std::copy(slot.hidden_target_addend.begin(), slot.hidden_target_addend.end(),
                      buf_hidden_addend_.begin() + a * h_dim);
        }
        buf_hidden_alphas_[a] = slot.hidden_alpha;
        buf_action_alphas_[a] = slot.action_alpha;
        // hidden_io / action_io are IO; load current state.
        std::copy(slot.hidden_io.begin(), slot.hidden_io.end(),
                  buf_hidden_io_.begin() + a * h_dim);
        std::copy(slot.action_io.begin(), slot.action_io.end(),
                  buf_action_io_.begin() + a * a_dim);
    }

    bool gpu_ok = false;
    const bool prefer_gpu = (active_backend_ == Backend::kGpu)
                            || (backend_choice_ == Backend::kAuto && gpu_.ready());
    if (prefer_gpu) {
        gpu_ok = gpu_.forward(batch, in_dim, h_dim, a_dim, i_dim,
                              buf_inputs_, buf_weights_ih_, buf_bias_h_,
                              buf_weights_ha_, buf_bias_a_,
                              buf_weights_hi_, buf_bias_i_,
                              buf_hidden_addend_,
                              buf_hidden_alphas_, buf_action_alphas_,
                              buf_hidden_io_, buf_action_io_, buf_interface_out_);
    }
    if (!gpu_ok) {
        BatchedDenseGpu::forwardCpu(batch, in_dim, h_dim, a_dim, i_dim,
                                    buf_inputs_, buf_weights_ih_, buf_bias_h_,
                                    buf_weights_ha_, buf_bias_a_,
                                    buf_weights_hi_, buf_bias_i_,
                                    buf_hidden_addend_,
                                    buf_hidden_alphas_, buf_action_alphas_,
                                    buf_hidden_io_, buf_action_io_, buf_interface_out_);
    }

    // Write batched outputs back into each agent's dense state. Hidden/action
    // are integrated in-place; interface is overwritten.
    for (std::size_t a = 0; a < batch; ++a) {
        const auto slot = brains[a]->denseSlot();
        std::copy(buf_hidden_io_.begin() + a * h_dim,
                  buf_hidden_io_.begin() + (a + 1) * h_dim,
                  slot.hidden_io.begin());
        std::copy(buf_action_io_.begin() + a * a_dim,
                  buf_action_io_.begin() + (a + 1) * a_dim,
                  slot.action_io.begin());
        std::copy(buf_interface_out_.begin() + a * i_dim,
                  buf_interface_out_.begin() + (a + 1) * i_dim,
                  slot.interface_out.begin());
    }

    last_batch_size_ = batch;
    last_batch_used_gpu_ = gpu_ok;
}

}  // namespace neuro::routes::protagonist
