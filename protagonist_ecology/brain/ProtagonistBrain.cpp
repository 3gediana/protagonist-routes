#include "brain/ProtagonistBrain.h"

#include "brain/ProtagonistDenseGpu.h"
#include "brain/ProtagonistGenome.h"
#include "runtime/TrainingProfiler.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <stdexcept>

namespace neuro::routes::protagonist {

namespace {

float integrationAlpha(float dt, float tau) {
    const float safe_dt = std::max(dt, 1.0e-4f);
    const float safe_tau = std::max(tau, 1.0e-4f);
    return std::clamp(safe_dt / safe_tau, 0.0f, 1.0f);
}

void fillRandom(std::vector<float>& data, std::mt19937& rng, float scale = 0.35f) {
    std::normal_distribution<float> dist(0.0f, scale);
    for (float& value : data) {
        value = dist(rng);
    }
}

// Deterministic 64-bit mixer (splitmix64). Used to derive the fixed sign
// pattern for the genetic-fingerprint random projection so every brain in a
// run shares the same projection and the fingerprints stay comparable.
std::uint64_t splitmix64(std::uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

void addWrapped(std::span<float> target,
                std::span<const float> source,
                float scale) {
    if (target.empty() || source.empty() || scale == 0.0f) {
        return;
    }
    for (std::size_t i = 0; i < target.size(); ++i) {
        target[i] += source[i % source.size()] * scale;
    }
}

}  // namespace

namespace {

float sigmoid(float x) {
    if (x >= 0.0f) {
        const float z = std::exp(-x);
        return 1.0f / (1.0f + z);
    }
    const float z = std::exp(x);
    return z / (1.0f + z);
}

float softplus(float x) {
    if (x > 20.0f) {
        return x;
    }
    if (x < -20.0f) {
        return std::exp(x);
    }
    return std::log1p(std::exp(x));
}

void softmax3(float& a, float& b, float& c) {
    const float max_value = std::max({a, b, c});
    a = std::exp(a - max_value);
    b = std::exp(b - max_value);
    c = std::exp(c - max_value);
    const float sum = a + b + c;
    if (sum <= 1.0e-6f) {
        a = 1.0f / 3.0f;
        b = 1.0f / 3.0f;
        c = 1.0f / 3.0f;
        return;
    }
    a /= sum;
    b /= sum;
    c /= sum;
}

DNCMemoryInterface decodeInterface(std::span<const float> raw, std::size_t word_size, std::size_t read_heads) {
    DNCMemoryInterface interface_state;
    interface_state.write_key.resize(word_size, 0.0f);
    interface_state.erase_vector.resize(word_size, 0.0f);
    interface_state.write_vector.resize(word_size, 0.0f);
    interface_state.free_gates.resize(read_heads, 0.0f);
    interface_state.read_keys.resize(read_heads * word_size, 0.0f);
    interface_state.read_strengths.resize(read_heads, 0.0f);
    interface_state.read_modes.resize(read_heads * 3, 0.0f);

    std::size_t cursor = 0;
    auto next = [&](float fallback = 0.0f) {
        if (cursor >= raw.size()) {
            return fallback;
        }
        return raw[cursor++];
    };

    for (std::size_t i = 0; i < word_size; ++i) {
        interface_state.write_key[i] = std::tanh(next());
    }
    interface_state.write_strength = softplus(next());
    for (std::size_t i = 0; i < word_size; ++i) {
        interface_state.erase_vector[i] = sigmoid(next());
    }
    for (std::size_t i = 0; i < word_size; ++i) {
        interface_state.write_vector[i] = std::tanh(next());
    }
    for (std::size_t i = 0; i < read_heads; ++i) {
        interface_state.free_gates[i] = sigmoid(next());
    }
    interface_state.allocation_gate = sigmoid(next());
    interface_state.write_gate = sigmoid(next());
    for (std::size_t i = 0; i < read_heads * word_size; ++i) {
        interface_state.read_keys[i] = std::tanh(next());
    }
    for (std::size_t i = 0; i < read_heads; ++i) {
        interface_state.read_strengths[i] = softplus(next());
    }
    for (std::size_t head = 0; head < read_heads; ++head) {
        float backward = next();
        float content = next();
        float forward = next();
        softmax3(backward, content, forward);
        interface_state.read_modes[head * 3 + 0] = backward;
        interface_state.read_modes[head * 3 + 1] = content;
        interface_state.read_modes[head * 3 + 2] = forward;
    }

    return interface_state;
}

float averageOf(const std::vector<float>& values) {
    if (values.empty()) {
        return 0.0f;
    }
    float sum = 0.0f;
    for (const float value : values) {
        sum += value;
    }
    return sum / static_cast<float>(values.size());
}

float maxOf(const std::vector<float>& values) {
    if (values.empty()) {
        return 0.0f;
    }
    return *std::max_element(values.begin(), values.end());
}

float l2NormOf(const std::vector<float>& values) {
    float sum = 0.0f;
    for (const float value : values) {
        sum += value * value;
    }
    return std::sqrt(sum);
}

float normalizedFocusSlot(const std::vector<float>& values, std::size_t stride) {
    if (values.empty() || stride == 0 || stride == 1) {
        return 0.0f;
    }

    const std::size_t groups = values.size() / stride;
    if (groups == 0) {
        return 0.0f;
    }

    float out = 0.0f;
    for (std::size_t group = 0; group < groups; ++group) {
        const auto begin = values.begin() + static_cast<std::ptrdiff_t>(group * stride);
        const auto end = begin + static_cast<std::ptrdiff_t>(stride);
        const auto it = std::max_element(begin, end);
        const auto index = static_cast<std::size_t>(std::distance(begin, it));
        out += static_cast<float>(index) / static_cast<float>(stride - 1);
    }
    return out / static_cast<float>(groups);
}

}  // namespace

ProtagonistBrain::ProtagonistBrain(std::size_t input_dim,
                                   std::size_t hidden_dim,
                                   std::size_t memory_cells,
                                   std::size_t memory_slots,
                                   std::size_t read_heads,
                                   std::size_t action_dim,
                                   std::uint32_t seed,
                                   bool enable_gpu_dense)
    : external_input_dim_(input_dim),
      total_input_dim_(input_dim + memory_cells * read_heads),
      hidden_dim_(hidden_dim),
      memory_cells_(memory_cells),
      memory_slots_(memory_slots),
      read_heads_(read_heads),
      action_dim_(action_dim),
      in_hidden_weights_(total_input_dim_ * hidden_dim_, 0.0f),
      hidden_bias_(hidden_dim_, 0.0f),
      hidden_action_weights_(hidden_dim_ * action_dim_, 0.0f),
      action_bias_(action_dim_, 0.0f),
      hidden_interface_weights_(hidden_dim_ * DNCMemory::interfaceDim(memory_cells_, read_heads_), 0.0f),
      interface_bias_(DNCMemory::interfaceDim(memory_cells_, read_heads_), 0.0f),
      observation_encoder_(external_input_dim_),
      memory_query_encoder_(memory_cells_ * read_heads_),
      body_state_memory_(external_input_dim_),
      working_memory_(hidden_dim_),
      behavior_fusion_trunk_(total_input_dim_, hidden_dim_),
      motor_core_(hidden_dim_),
      action_heads_(hidden_dim_, action_dim_),
      interface_head_(hidden_dim_, DNCMemory::interfaceDim(memory_cells_, read_heads_)),
      dnc_memory_(memory_slots_, memory_cells_, read_heads_),
      spatial_memory_(32, 32, 200.0f, 200.0f),
      episodic_memory_(32),
      social_memory_(32),
      goal_memory_() {
    if (input_dim == 0 || hidden_dim == 0 || memory_cells == 0 || memory_slots == 0 || read_heads == 0 || action_dim == 0) {
        throw std::invalid_argument("ProtagonistBrain dims must be > 0");
    }

    std::mt19937 rng(seed == 0 ? std::random_device{}() : seed);
    fillRandom(in_hidden_weights_, rng);
    fillRandom(hidden_bias_, rng);
    fillRandom(hidden_action_weights_, rng);
    fillRandom(action_bias_, rng);
    fillRandom(hidden_interface_weights_, rng);
    fillRandom(interface_bias_, rng);

    if (enable_gpu_dense) {
        dense_gpu_ = std::make_unique<ProtagonistDenseGpu>(total_input_dim_,
                                                           hidden_dim_,
                                                           action_dim_,
                                                           interface_bias_.size(),
                                                           in_hidden_weights_,
                                                           hidden_bias_,
                                                           hidden_action_weights_,
                                                           action_bias_,
                                                           hidden_interface_weights_,
                                                           interface_bias_);
        if (!dense_gpu_->ready()) {
            dense_gpu_.reset();
        }
    }

    computeGeneticFingerprint();
}

ProtagonistBrain::ProtagonistBrain(const ProtagonistGenome& genome, bool enable_gpu_dense)
    : external_input_dim_(genome.inputDim()),
      total_input_dim_(genome.inputDim() + genome.memoryCells() * genome.readHeads()),
      hidden_dim_(genome.hiddenDim()),
      memory_cells_(genome.memoryCells()),
      memory_slots_(genome.memorySlots()),
      read_heads_(genome.readHeads()),
      action_dim_(genome.actionDim()),
      in_hidden_weights_(genome.inputHiddenWeights()),
      hidden_bias_(genome.hiddenBias()),
      hidden_action_weights_(genome.hiddenActionWeights()),
      action_bias_(genome.actionBias()),
      hidden_interface_weights_(genome.hiddenInterfaceWeights()),
      interface_bias_(genome.interfaceBias()),
      observation_encoder_(external_input_dim_),
      memory_query_encoder_(memory_cells_ * read_heads_),
      body_state_memory_(external_input_dim_),
      working_memory_(hidden_dim_),
      behavior_fusion_trunk_(total_input_dim_, hidden_dim_),
      motor_core_(hidden_dim_),
      action_heads_(hidden_dim_, action_dim_),
      interface_head_(hidden_dim_, interface_bias_.size()),
      dnc_memory_(genome.memorySlots(), genome.memoryCells(), genome.readHeads()),
      spatial_memory_(32, 32, 200.0f, 200.0f),
      episodic_memory_(32),
      social_memory_(32),
      goal_memory_() {
    if (enable_gpu_dense) {
        dense_gpu_ = std::make_unique<ProtagonistDenseGpu>(total_input_dim_,
                                                           hidden_dim_,
                                                           action_dim_,
                                                           interface_bias_.size(),
                                                           in_hidden_weights_,
                                                           hidden_bias_,
                                                           hidden_action_weights_,
                                                           action_bias_,
                                                           hidden_interface_weights_,
                                                           interface_bias_);
        if (!dense_gpu_->ready()) {
            dense_gpu_.reset();
        }
    }

    computeGeneticFingerprint();
}

void ProtagonistBrain::computeGeneticFingerprint() {
    // Sign random projection (Achlioptas) of the input->hidden weight vector to
    // a low-dim fingerprint. To bound construction cost we sample at most
    // kMaxSamples weights at a fixed stride (identical across brains in a run,
    // since they share total_input_dim_/hidden_dim_), which preserves cosine.
    constexpr std::uint64_t kProjSeed = 0x4B494E2D44ull;  // "KIN-D"
    constexpr std::size_t kMaxSamples = 4096;
    genetic_fingerprint_.assign(kGeneticFingerprintDim, 0.0f);

    const std::vector<float>& w = in_hidden_weights_;
    if (w.empty()) {
        return;
    }
    const std::size_t stride = std::max<std::size_t>(1, w.size() / kMaxSamples);

    for (std::size_t d = 0; d < kGeneticFingerprintDim; ++d) {
        float acc = 0.0f;
        const std::uint64_t row_seed = kProjSeed ^ (static_cast<std::uint64_t>(d) << 40);
        for (std::size_t i = 0; i < w.size(); i += stride) {
            const std::uint64_t h = splitmix64(row_seed ^ static_cast<std::uint64_t>(i));
            acc += (h & 1ull) ? w[i] : -w[i];
        }
        genetic_fingerprint_[d] = acc;
    }

    float norm = 0.0f;
    for (const float v : genetic_fingerprint_) {
        norm += v * v;
    }
    norm = std::sqrt(norm);
    if (norm > 1.0e-8f) {
        for (float& v : genetic_fingerprint_) {
            v /= norm;
        }
    }
}

ProtagonistBrain::~ProtagonistBrain() = default;

std::size_t ProtagonistBrain::inputDim() const {
    return external_input_dim_;
}

std::size_t ProtagonistBrain::outputDim() const {
    return action_dim_;
}

DNCStateSummary ProtagonistBrain::dncSummary() const {
    DNCStateSummary summary;
    summary.usage_mean = averageOf(dnc_memory_.usage());
    summary.usage_max = maxOf(dnc_memory_.usage());
    summary.write_peak = maxOf(dnc_memory_.writeWeighting());
    summary.read_peak = maxOf(dnc_memory_.readWeightings());
    summary.read_norm = l2NormOf(dnc_memory_.readVectors());
    summary.precedence_peak = maxOf(dnc_memory_.precedence());
    summary.write_focus_slot = normalizedFocusSlot(dnc_memory_.writeWeighting(), dnc_memory_.slotCount());
    summary.read_focus_slot = normalizedFocusSlot(dnc_memory_.readWeightings(), dnc_memory_.slotCount());
    return summary;
}

std::span<const float> ProtagonistBrain::forward(std::span<const float> inputs, float dt) {
    TrainingProfiler::Scope profile_scope(TrainingProfiler::Phase::kBrainForward);
    if (inputs.size() != external_input_dim_) {
        throw std::invalid_argument("ProtagonistBrain input dim mismatch");
    }

    const float hidden_alpha = integrationAlpha(dt, 0.25f);
    const float action_alpha = integrationAlpha(dt, 0.12f);

    // Step V2 memory banks
    spatial_memory_.tick(dt);
    social_memory_.tick(dt);
    goal_memory_.tick(dt);

    const auto encoded_observation = observation_encoder_.encode(inputs);
    body_state_memory_.observe(encoded_observation, dt);
    const auto encoded_queries = memory_query_encoder_.encode(dnc_memory_.readVectors());

    std::vector<float> full_input(total_input_dim_, 0.0f);
    std::copy(encoded_observation.begin(), encoded_observation.end(), full_input.begin());
    std::copy(encoded_queries.begin(), encoded_queries.end(), full_input.begin() + static_cast<std::ptrdiff_t>(external_input_dim_));
    addWrapped(std::span<float>(full_input).first(external_input_dim_),
               body_state_memory_.summary(),
               0.25f);
    addWrapped(std::span<float>(full_input).subspan(external_input_dim_),
               working_memory_.summary(),
               0.20f);

    if (dense_gpu_ != nullptr) {
        if (dense_gpu_->forward(full_input,
                                hidden_alpha,
                                action_alpha,
                                motor_core_.mutableState(),
                                action_heads_.mutableOutputs(),
                                interface_head_.mutableOutput())) {
            working_memory_.observe(motor_core_.state(), dt);
            dnc_memory_.step(decodeInterface(interface_head_.output(), memory_cells_, read_heads_));
            return action_heads_.outputs();
        }
        dense_gpu_.reset();
    }

    const auto fused_hidden = behavior_fusion_trunk_.forward(full_input, in_hidden_weights_, hidden_bias_);
    std::vector<float> motor_target(fused_hidden.begin(), fused_hidden.end());
    addWrapped(std::span<float>(motor_target),
               working_memory_.summary(),
               0.15f);
    const auto motor_state = motor_core_.integrate(motor_target, dt);
    working_memory_.observe(motor_state, dt);
    const auto outputs = action_heads_.forward(motor_state, hidden_action_weights_, action_bias_, dt);
    const auto interface_vector = interface_head_.forward(motor_state, hidden_interface_weights_, interface_bias_);

    dnc_memory_.step(decodeInterface(interface_vector, memory_cells_, read_heads_));

    return outputs;
}

// IBatchableBrain ============================================================
// Phase 1 (per-agent parallel): same prologue as forward(), but stops after
// building full_input. The dense matmul is deferred to denseSlot()/external
// batched runner. Memory bank ticks and observation_encoder updates remain
// here because they must happen exactly once per decision tick, regardless
// of whether the dense matmul is batched.
void ProtagonistBrain::prepareInputs(std::span<const float> external_inputs, float dt) {
    TrainingProfiler::Scope profile_scope(TrainingProfiler::Phase::kBrainForward);
    if (external_inputs.size() != external_input_dim_) {
        throw std::invalid_argument("ProtagonistBrain input dim mismatch (prepareInputs)");
    }

    pending_hidden_alpha_ = integrationAlpha(dt, 0.25f);
    pending_action_alpha_ = integrationAlpha(dt, 0.12f);

    spatial_memory_.tick(dt);
    social_memory_.tick(dt);
    goal_memory_.tick(dt);

    const auto encoded_observation = observation_encoder_.encode(external_inputs);
    body_state_memory_.observe(encoded_observation, dt);
    const auto encoded_queries = memory_query_encoder_.encode(dnc_memory_.readVectors());

    pending_full_input_.assign(total_input_dim_, 0.0f);
    std::copy(encoded_observation.begin(), encoded_observation.end(), pending_full_input_.begin());
    std::copy(encoded_queries.begin(),
              encoded_queries.end(),
              pending_full_input_.begin() + static_cast<std::ptrdiff_t>(external_input_dim_));
    addWrapped(std::span<float>(pending_full_input_).first(external_input_dim_),
               body_state_memory_.summary(),
               0.25f);
    addWrapped(std::span<float>(pending_full_input_).subspan(external_input_dim_),
               working_memory_.summary(),
               0.20f);

    // Match the CPU full-path forward(): the working-memory summary is injected
    // into motor_target (post-trunk, pre-integration) with scale 0.15. The
    // batched dense runner applies this as a per-hidden addend. Read the summary
    // here (pre-observe, exactly as forward() does) so the addend is computed
    // from the same state the serial path uses.
    pending_hidden_addend_.assign(hidden_dim_, 0.0f);
    addWrapped(std::span<float>(pending_hidden_addend_),
               working_memory_.summary(),
               0.15f);
}

// Phase 2 hand-off: runner reads inputs / weights / biases, writes hidden /
// action / interface mutable spans. After the runner returns, finalizeOutputs()
// is called.
BatchedDenseSlot ProtagonistBrain::denseSlot() {
    BatchedDenseSlot slot;
    slot.total_input_dim = total_input_dim_;
    slot.hidden_dim = hidden_dim_;
    slot.action_dim = action_dim_;
    slot.interface_dim = interface_bias_.size();
    slot.hidden_alpha = pending_hidden_alpha_;
    slot.action_alpha = pending_action_alpha_;

    slot.full_input = std::span<const float>(pending_full_input_);
    slot.hidden_target_addend = std::span<const float>(pending_hidden_addend_);

    slot.input_hidden_weights = std::span<const float>(in_hidden_weights_);
    slot.hidden_bias = std::span<const float>(hidden_bias_);
    slot.hidden_action_weights = std::span<const float>(hidden_action_weights_);
    slot.action_bias = std::span<const float>(action_bias_);
    slot.hidden_interface_weights = std::span<const float>(hidden_interface_weights_);
    slot.interface_bias = std::span<const float>(interface_bias_);

    slot.hidden_io = motor_core_.mutableState();
    slot.action_io = action_heads_.mutableOutputs();
    slot.interface_out = interface_head_.mutableOutput();

    return slot;
}

// Phase 3 (per-agent parallel): dense state has been written by the runner.
// Run the same epilogue as the GPU branch in forward(): working memory observe,
// DNC step, return action span.
std::span<const float> ProtagonistBrain::finalizeOutputs(float dt) {
    TrainingProfiler::Scope profile_scope(TrainingProfiler::Phase::kBrainForward);
    working_memory_.observe(motor_core_.state(), dt);
    dnc_memory_.step(decodeInterface(interface_head_.output(), memory_cells_, read_heads_));
    return action_heads_.outputs();
}

void ProtagonistBrain::reset() {
    observation_encoder_.reset();
    memory_query_encoder_.reset();
    body_state_memory_.reset();
    working_memory_.reset();
    behavior_fusion_trunk_.reset();
    motor_core_.reset();
    action_heads_.reset();
    interface_head_.reset();
    if (dense_gpu_ != nullptr) {
        dense_gpu_->reset();
    }
    dnc_memory_.reset();
    spatial_memory_.reset();
    episodic_memory_.reset();
    social_memory_.reset();
    goal_memory_.reset();
}

std::size_t ProtagonistBrain::parameterCount() const {
    return in_hidden_weights_.size()
         + hidden_bias_.size()
         + hidden_action_weights_.size()
         + action_bias_.size()
         + hidden_interface_weights_.size()
         + interface_bias_.size();
}

}  // namespace neuro::routes::protagonist
