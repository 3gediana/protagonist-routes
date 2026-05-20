#include "brain/ProtagonistBrain.h"

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

ProtagonistBrain::ProtagonistBrain(std::size_t input_dim,
                                   std::size_t hidden_dim,
                                   std::size_t memory_cells,
                                   std::size_t action_dim,
                                   std::uint32_t seed)
    : external_input_dim_(input_dim),
      total_input_dim_(input_dim + memory_cells),
      hidden_dim_(hidden_dim),
      action_dim_(action_dim),
      in_hidden_weights_(total_input_dim_ * hidden_dim_, 0.0f),
      hidden_bias_(hidden_dim_, 0.0f),
      hidden_action_weights_(hidden_dim_ * action_dim_, 0.0f),
      action_bias_(action_dim_, 0.0f),
      hidden_memory_candidate_weights_(hidden_dim_ * memory_cells, 0.0f),
      memory_candidate_bias_(memory_cells, 0.0f),
      hidden_memory_gate_weights_(hidden_dim_ * memory_cells, 0.0f),
      memory_gate_bias_(memory_cells, 0.0f),
      hidden_(hidden_dim_, 0.0f),
      outputs_(action_dim_, 0.0f),
      memory_(memory_cells),
      spatial_memory_(32, 32, 200.0f, 200.0f),
      episodic_memory_(32),
      social_memory_(32),
      goal_memory_() {
    if (input_dim == 0 || hidden_dim == 0 || memory_cells == 0 || action_dim == 0) {
        throw std::invalid_argument("ProtagonistBrain dims must be > 0");
    }

    std::mt19937 rng(seed == 0 ? std::random_device{}() : seed);
    fillRandom(in_hidden_weights_, rng);
    fillRandom(hidden_bias_, rng);
    fillRandom(hidden_action_weights_, rng);
    fillRandom(action_bias_, rng);
    fillRandom(hidden_memory_candidate_weights_, rng);
    fillRandom(memory_candidate_bias_, rng);
    fillRandom(hidden_memory_gate_weights_, rng);
    fillRandom(memory_gate_bias_, rng);
}

ProtagonistBrain::ProtagonistBrain(const ProtagonistGenome& genome)
    : external_input_dim_(genome.inputDim()),
      total_input_dim_(genome.inputDim() + genome.memoryCells()),
      hidden_dim_(genome.hiddenDim()),
      action_dim_(genome.actionDim()),
      in_hidden_weights_(genome.inputHiddenWeights()),
      hidden_bias_(genome.hiddenBias()),
      hidden_action_weights_(genome.hiddenActionWeights()),
      action_bias_(genome.actionBias()),
      hidden_memory_candidate_weights_(genome.hiddenMemoryCandidateWeights()),
      memory_candidate_bias_(genome.memoryCandidateBias()),
      hidden_memory_gate_weights_(genome.hiddenMemoryGateWeights()),
      memory_gate_bias_(genome.memoryGateBias()),
      hidden_(hidden_dim_, 0.0f),
      outputs_(action_dim_, 0.0f),
      memory_(genome.memoryCells()),
      spatial_memory_(32, 32, 200.0f, 200.0f),
      episodic_memory_(32),
      social_memory_(32),
      goal_memory_() {}

std::size_t ProtagonistBrain::inputDim() const {
    return external_input_dim_;
}

std::size_t ProtagonistBrain::outputDim() const {
    return action_dim_;
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
    const auto mem_values = memory_.values();
    std::copy(mem_values.begin(), mem_values.end(), full_input.begin() + static_cast<std::ptrdiff_t>(external_input_dim_));

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

    std::vector<float> candidates(memory_.cellCount(), 0.0f);
    std::vector<float> gates(memory_.cellCount(), 0.0f);
    for (std::size_t m = 0; m < memory_.cellCount(); ++m) {
        float candidate_sum = memory_candidate_bias_[m];
        float gate_sum = memory_gate_bias_[m];
        for (std::size_t h = 0; h < hidden_dim_; ++h) {
            candidate_sum += hidden_[h] * hidden_memory_candidate_weights_[h * memory_.cellCount() + m];
            gate_sum += hidden_[h] * hidden_memory_gate_weights_[h * memory_.cellCount() + m];
        }
        candidates[m] = activate(candidate_sum);
        gates[m] = 0.5f * (activate(gate_sum) + 1.0f);
    }

    const float decay = std::clamp(dt * 0.08f, 0.01f, 0.25f);
    memory_.update(candidates, gates, decay);

    return outputs_;
}

void ProtagonistBrain::reset() {
    std::fill(hidden_.begin(), hidden_.end(), 0.0f);
    std::fill(outputs_.begin(), outputs_.end(), 0.0f);
    memory_.reset();
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
         + hidden_memory_candidate_weights_.size()
         + memory_candidate_bias_.size()
         + hidden_memory_gate_weights_.size()
         + memory_gate_bias_.size();
}

}  // namespace neuro::routes::protagonist
