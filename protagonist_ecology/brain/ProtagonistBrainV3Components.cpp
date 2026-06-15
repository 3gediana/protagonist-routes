#include "brain/ProtagonistBrainV3Components.h"

#include <algorithm>
#include <cmath>
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

float meanAbs(std::span<const float> values) {
    if (values.empty()) {
        return 0.0f;
    }
    float sum = 0.0f;
    for (const float value : values) {
        sum += std::fabs(value);
    }
    return sum / static_cast<float>(values.size());
}

float maxAbs(std::span<const float> values) {
    float out = 0.0f;
    for (const float value : values) {
        out = std::max(out, std::fabs(value));
    }
    return out;
}

void fillWrapped(std::span<float> out, std::span<const float> src, float scale) {
    if (out.empty()) {
        return;
    }
    if (src.empty()) {
        std::fill(out.begin(), out.end(), 0.0f);
        return;
    }
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = std::tanh(src[i % src.size()] * scale);
    }
}

void fillGateWrapped(std::span<float> out, std::span<const float> src, float bias) {
    if (out.empty()) {
        return;
    }
    if (src.empty()) {
        std::fill(out.begin(), out.end(), 0.0f);
        return;
    }
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = std::clamp(std::fabs(src[i % src.size()]) + bias, 0.0f, 1.0f);
    }
}

std::size_t clampCells(std::size_t value, std::size_t lo, std::size_t hi) {
    return std::min(hi, std::max(lo, value));
}

}  // namespace

ObservationEncoder::ObservationEncoder(std::size_t input_dim)
    : output_(input_dim, 0.0f) {}

void ObservationEncoder::reset() {
    std::fill(output_.begin(), output_.end(), 0.0f);
}

std::span<const float> ObservationEncoder::encode(std::span<const float> raw_inputs) {
    if (raw_inputs.size() != output_.size()) {
        throw std::invalid_argument("ObservationEncoder input dim mismatch");
    }
    std::copy(raw_inputs.begin(), raw_inputs.end(), output_.begin());
    return output_;
}

MemoryQueryEncoder::MemoryQueryEncoder(std::size_t query_dim)
    : output_(query_dim, 0.0f) {}

void MemoryQueryEncoder::reset() {
    std::fill(output_.begin(), output_.end(), 0.0f);
}

std::span<const float> MemoryQueryEncoder::encode(std::span<const float> query_inputs) {
    if (query_inputs.size() != output_.size()) {
        throw std::invalid_argument("MemoryQueryEncoder input dim mismatch");
    }
    std::copy(query_inputs.begin(), query_inputs.end(), output_.begin());
    return output_;
}

BodyStateMemory::BodyStateMemory(std::size_t observation_dim)
    : summary_(std::min<std::size_t>(observation_dim, 16), 0.0f) {}

void BodyStateMemory::reset() {
    std::fill(summary_.begin(), summary_.end(), 0.0f);
}

void BodyStateMemory::observe(std::span<const float> observation, float dt) {
    const float alpha = integrationAlpha(dt, 1.5f);
    for (std::size_t i = 0; i < summary_.size(); ++i) {
        const float sample = i < observation.size() ? observation[i] : 0.0f;
        summary_[i] += alpha * (sample - summary_[i]);
    }
}

WorkingMemoryBank::WorkingMemoryBank(std::size_t hidden_dim)
    : fast_(clampCells(hidden_dim, 16, 64)),
      medium_(clampCells((hidden_dim + 1) / 2, 16, 64)),
      slow_(clampCells((hidden_dim + 3) / 4, 8, 32)),
      meta_(16),
      fast_candidates_(fast_.cellCount(), 0.0f),
      medium_candidates_(medium_.cellCount(), 0.0f),
      slow_candidates_(slow_.cellCount(), 0.0f),
      meta_candidates_(meta_.cellCount(), 0.0f),
      fast_gates_(fast_.cellCount(), 0.0f),
      medium_gates_(medium_.cellCount(), 0.0f),
      slow_gates_(slow_.cellCount(), 0.0f),
      meta_gates_(meta_.cellCount(), 0.0f),
      summary_(8, 0.0f) {}

void WorkingMemoryBank::reset() {
    fast_.reset();
    medium_.reset();
    slow_.reset();
    meta_.reset();
    std::fill(summary_.begin(), summary_.end(), 0.0f);
}

void WorkingMemoryBank::observe(std::span<const float> fused_hidden, float dt) {
    fillWrapped(fast_candidates_, fused_hidden, 1.0f);
    fillWrapped(medium_candidates_, fused_hidden, 0.75f);
    fillWrapped(slow_candidates_, fused_hidden, 0.5f);
    fillWrapped(meta_candidates_, fused_hidden, 0.25f);

    fillGateWrapped(fast_gates_, fused_hidden, 0.15f);
    fillGateWrapped(medium_gates_, fused_hidden, 0.10f);
    fillGateWrapped(slow_gates_, fused_hidden, 0.05f);
    fillGateWrapped(meta_gates_, fused_hidden, 0.20f);

    fast_.update(fast_candidates_, fast_gates_, integrationAlpha(dt, 0.30f));
    medium_.update(medium_candidates_, medium_gates_, integrationAlpha(dt, 0.90f));
    slow_.update(slow_candidates_, slow_gates_, integrationAlpha(dt, 3.00f));
    meta_.update(meta_candidates_, meta_gates_, integrationAlpha(dt, 1.50f));

    summary_[0] = meanAbs(fast_.values());
    summary_[1] = maxAbs(fast_.values());
    summary_[2] = meanAbs(medium_.values());
    summary_[3] = maxAbs(medium_.values());
    summary_[4] = meanAbs(slow_.values());
    summary_[5] = maxAbs(slow_.values());
    summary_[6] = meanAbs(meta_.values());
    summary_[7] = maxAbs(meta_.values());
}

std::size_t WorkingMemoryBank::totalCellCount() const {
    return fast_.cellCount() + medium_.cellCount() + slow_.cellCount() + meta_.cellCount();
}

BehaviorFusionTrunk::BehaviorFusionTrunk(std::size_t input_dim, std::size_t hidden_dim)
    : input_dim_(input_dim),
      hidden_dim_(hidden_dim),
      hidden_target_(hidden_dim, 0.0f) {}

void BehaviorFusionTrunk::reset() {
    std::fill(hidden_target_.begin(), hidden_target_.end(), 0.0f);
}

std::span<const float> BehaviorFusionTrunk::forward(std::span<const float> fused_input,
                                                    std::span<const float> input_hidden_weights,
                                                    std::span<const float> hidden_bias) {
    if (fused_input.size() != input_dim_
        || input_hidden_weights.size() != input_dim_ * hidden_dim_
        || hidden_bias.size() != hidden_dim_) {
        throw std::invalid_argument("BehaviorFusionTrunk shape mismatch");
    }

    for (std::size_t h = 0; h < hidden_dim_; ++h) {
        float sum = hidden_bias[h];
        for (std::size_t i = 0; i < input_dim_; ++i) {
            sum += fused_input[i] * input_hidden_weights[i * hidden_dim_ + h];
        }
        hidden_target_[h] = activate(sum);
    }
    return hidden_target_;
}

MotorCore::MotorCore(std::size_t hidden_dim)
    : state_(hidden_dim, 0.0f) {}

void MotorCore::reset() {
    std::fill(state_.begin(), state_.end(), 0.0f);
}

std::span<const float> MotorCore::integrate(std::span<const float> target, float dt) {
    if (target.size() != state_.size()) {
        throw std::invalid_argument("MotorCore hidden dim mismatch");
    }
    const float alpha = integrationAlpha(dt, 0.25f);
    for (std::size_t i = 0; i < state_.size(); ++i) {
        state_[i] += alpha * (target[i] - state_[i]);
    }
    return state_;
}

ActionHeads::ActionHeads(std::size_t hidden_dim, std::size_t action_dim)
    : hidden_dim_(hidden_dim),
      action_dim_(action_dim),
      output_target_(action_dim, 0.0f),
      outputs_(action_dim, 0.0f) {}

void ActionHeads::reset() {
    std::fill(output_target_.begin(), output_target_.end(), 0.0f);
    std::fill(outputs_.begin(), outputs_.end(), 0.0f);
}

std::span<const float> ActionHeads::forward(std::span<const float> motor_state,
                                            std::span<const float> hidden_action_weights,
                                            std::span<const float> action_bias,
                                            float dt) {
    if (motor_state.size() != hidden_dim_
        || hidden_action_weights.size() != hidden_dim_ * action_dim_
        || action_bias.size() != action_dim_) {
        throw std::invalid_argument("ActionHeads shape mismatch");
    }

    const float alpha = integrationAlpha(dt, 0.12f);
    for (std::size_t o = 0; o < action_dim_; ++o) {
        float sum = action_bias[o];
        for (std::size_t h = 0; h < hidden_dim_; ++h) {
            sum += motor_state[h] * hidden_action_weights[h * action_dim_ + o];
        }
        output_target_[o] = activate(sum);
        outputs_[o] += alpha * (output_target_[o] - outputs_[o]);
    }
    return outputs_;
}

InterfaceHead::InterfaceHead(std::size_t hidden_dim, std::size_t interface_dim)
    : hidden_dim_(hidden_dim),
      interface_dim_(interface_dim),
      output_(interface_dim, 0.0f) {}

void InterfaceHead::reset() {
    std::fill(output_.begin(), output_.end(), 0.0f);
}

std::span<const float> InterfaceHead::forward(std::span<const float> motor_state,
                                              std::span<const float> hidden_interface_weights,
                                              std::span<const float> interface_bias) {
    if (motor_state.size() != hidden_dim_
        || hidden_interface_weights.size() != hidden_dim_ * interface_dim_
        || interface_bias.size() != interface_dim_) {
        throw std::invalid_argument("InterfaceHead shape mismatch");
    }

    for (std::size_t i = 0; i < interface_dim_; ++i) {
        float sum = interface_bias[i];
        for (std::size_t h = 0; h < hidden_dim_; ++h) {
            sum += motor_state[h] * hidden_interface_weights[h * interface_dim_ + i];
        }
        output_[i] = sum;
    }
    return output_;
}

}  // namespace neuro::routes::protagonist
