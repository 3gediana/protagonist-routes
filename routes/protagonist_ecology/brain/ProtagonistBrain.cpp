#include "brain/ProtagonistBrain.h"

#include "brain/ProtagonistDenseGpu.h"
#include "brain/ProtagonistGenome.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>

namespace neuro::routes::protagonist {

namespace {

float activate(float x) {
    return std::tanh(x);
}

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
      hidden_(hidden_dim_, 0.0f),
      outputs_(action_dim_, 0.0f),
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
      hidden_(hidden_dim_, 0.0f),
      outputs_(action_dim_, 0.0f),
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
    if (inputs.size() != external_input_dim_) {
        throw std::invalid_argument("ProtagonistBrain input dim mismatch");
    }

    const float hidden_alpha = integrationAlpha(dt, 0.25f);
    const float action_alpha = integrationAlpha(dt, 0.12f);

    // Step V2 memory banks
    spatial_memory_.tick(dt);
    social_memory_.tick(dt);
    goal_memory_.tick(dt);

    std::vector<float> full_input(total_input_dim_, 0.0f);
    std::copy(inputs.begin(), inputs.end(), full_input.begin());
    const auto& read_vectors = dnc_memory_.readVectors();
    std::copy(read_vectors.begin(), read_vectors.end(), full_input.begin() + static_cast<std::ptrdiff_t>(external_input_dim_));

    std::vector<float> interface_vector(interface_bias_.size(), 0.0f);
    if (dense_gpu_ != nullptr) {
        if (dense_gpu_->forward(full_input, hidden_alpha, action_alpha, hidden_, outputs_, interface_vector)) {
            dnc_memory_.step(decodeInterface(interface_vector, memory_cells_, read_heads_));
            return outputs_;
        }
        dense_gpu_.reset();
    }

    std::vector<float> hidden_target(hidden_dim_, 0.0f);
    for (std::size_t h = 0; h < hidden_dim_; ++h) {
        float sum = hidden_bias_[h];
        for (std::size_t i = 0; i < total_input_dim_; ++i) {
            sum += full_input[i] * in_hidden_weights_[i * hidden_dim_ + h];
        }
        hidden_target[h] = activate(sum);
        hidden_[h] += hidden_alpha * (hidden_target[h] - hidden_[h]);
    }

    std::vector<float> output_target(action_dim_, 0.0f);
    for (std::size_t o = 0; o < action_dim_; ++o) {
        float sum = action_bias_[o];
        for (std::size_t h = 0; h < hidden_dim_; ++h) {
            sum += hidden_[h] * hidden_action_weights_[h * action_dim_ + o];
        }
        output_target[o] = activate(sum);
        outputs_[o] += action_alpha * (output_target[o] - outputs_[o]);
    }

    for (std::size_t i = 0; i < interface_vector.size(); ++i) {
        float sum = interface_bias_[i];
        for (std::size_t h = 0; h < hidden_dim_; ++h) {
            sum += hidden_[h] * hidden_interface_weights_[h * interface_vector.size() + i];
        }
        interface_vector[i] = sum;
    }

    dnc_memory_.step(decodeInterface(interface_vector, memory_cells_, read_heads_));

    return outputs_;
}

void ProtagonistBrain::reset() {
    std::fill(hidden_.begin(), hidden_.end(), 0.0f);
    std::fill(outputs_.begin(), outputs_.end(), 0.0f);
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
