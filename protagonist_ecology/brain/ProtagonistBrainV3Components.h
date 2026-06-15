#pragma once

#include "brain/MemoryBank.h"

#include <cstddef>
#include <span>
#include <vector>

namespace neuro::routes::protagonist {

class ObservationEncoder final {
public:
    explicit ObservationEncoder(std::size_t input_dim);

    void reset();
    std::span<const float> encode(std::span<const float> raw_inputs);
    std::size_t outputDim() const { return output_.size(); }
    std::span<const float> output() const { return std::span<const float>(output_); }

private:
    std::vector<float> output_;
};

class MemoryQueryEncoder final {
public:
    explicit MemoryQueryEncoder(std::size_t query_dim);

    void reset();
    std::span<const float> encode(std::span<const float> query_inputs);
    std::size_t outputDim() const { return output_.size(); }
    std::span<const float> output() const { return std::span<const float>(output_); }

private:
    std::vector<float> output_;
};

class BodyStateMemory final {
public:
    explicit BodyStateMemory(std::size_t observation_dim);

    void reset();
    void observe(std::span<const float> observation, float dt);
    std::size_t summaryDim() const { return summary_.size(); }
    std::span<const float> summary() const { return std::span<const float>(summary_); }

private:
    std::vector<float> summary_;
};

class WorkingMemoryBank final {
public:
    explicit WorkingMemoryBank(std::size_t hidden_dim);

    void reset();
    void observe(std::span<const float> fused_hidden, float dt);
    std::size_t totalCellCount() const;
    std::span<const float> summary() const { return std::span<const float>(summary_); }

private:
    MemoryBank fast_;
    MemoryBank medium_;
    MemoryBank slow_;
    MemoryBank meta_;
    std::vector<float> fast_candidates_;
    std::vector<float> medium_candidates_;
    std::vector<float> slow_candidates_;
    std::vector<float> meta_candidates_;
    std::vector<float> fast_gates_;
    std::vector<float> medium_gates_;
    std::vector<float> slow_gates_;
    std::vector<float> meta_gates_;
    std::vector<float> summary_;
};

class BehaviorFusionTrunk final {
public:
    BehaviorFusionTrunk(std::size_t input_dim, std::size_t hidden_dim);

    void reset();
    std::span<const float> forward(std::span<const float> fused_input,
                                   std::span<const float> input_hidden_weights,
                                   std::span<const float> hidden_bias);
    std::size_t hiddenDim() const { return hidden_target_.size(); }

private:
    std::size_t input_dim_;
    std::size_t hidden_dim_;
    std::vector<float> hidden_target_;
};

class MotorCore final {
public:
    explicit MotorCore(std::size_t hidden_dim);

    void reset();
    std::span<const float> integrate(std::span<const float> target, float dt);
    std::size_t hiddenDim() const { return state_.size(); }
    std::span<float> mutableState() { return std::span<float>(state_); }
    std::span<const float> state() const { return std::span<const float>(state_); }

private:
    std::vector<float> state_;
};

class ActionHeads final {
public:
    ActionHeads(std::size_t hidden_dim, std::size_t action_dim);

    void reset();
    std::span<const float> forward(std::span<const float> motor_state,
                                   std::span<const float> hidden_action_weights,
                                   std::span<const float> action_bias,
                                   float dt);
    std::size_t actionDim() const { return outputs_.size(); }
    std::span<float> mutableOutputs() { return std::span<float>(outputs_); }
    std::span<const float> outputs() const { return std::span<const float>(outputs_); }

private:
    std::size_t hidden_dim_;
    std::size_t action_dim_;
    std::vector<float> output_target_;
    std::vector<float> outputs_;
};

class InterfaceHead final {
public:
    InterfaceHead(std::size_t hidden_dim, std::size_t interface_dim);

    void reset();
    std::span<const float> forward(std::span<const float> motor_state,
                                   std::span<const float> hidden_interface_weights,
                                   std::span<const float> interface_bias);
    std::size_t interfaceDim() const { return output_.size(); }
    std::span<float> mutableOutput() { return std::span<float>(output_); }
    std::span<const float> output() const { return std::span<const float>(output_); }

private:
    std::size_t hidden_dim_;
    std::size_t interface_dim_;
    std::vector<float> output_;
};

}  // namespace neuro::routes::protagonist
