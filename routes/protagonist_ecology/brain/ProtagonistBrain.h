#pragma once

#include "brain/MemoryBank.h"
#include "core/interfaces/IBrain.h"

#include <vector>

namespace neuro::routes::protagonist {

class ProtagonistGenome;

class ProtagonistBrain final : public IBrain {
public:
    ProtagonistBrain(std::size_t input_dim,
                     std::size_t hidden_dim,
                     std::size_t memory_cells,
                     std::size_t action_dim,
                     std::uint32_t seed);
    explicit ProtagonistBrain(const ProtagonistGenome& genome);

    std::size_t inputDim() const override;
    std::size_t outputDim() const override;
    std::span<const float> forward(std::span<const float> inputs, float dt) override;
    void reset() override;
    std::size_t parameterCount() const override;

    std::size_t hiddenDim() const { return hidden_dim_; }
    std::size_t memoryCells() const { return memory_.cellCount(); }

private:
    std::size_t external_input_dim_;
    std::size_t total_input_dim_;
    std::size_t hidden_dim_;
    std::size_t action_dim_;
    std::vector<float> in_hidden_weights_;
    std::vector<float> hidden_bias_;
    std::vector<float> hidden_action_weights_;
    std::vector<float> action_bias_;
    std::vector<float> hidden_memory_candidate_weights_;
    std::vector<float> memory_candidate_bias_;
    std::vector<float> hidden_memory_gate_weights_;
    std::vector<float> memory_gate_bias_;
    std::vector<float> hidden_;
    std::vector<float> outputs_;
    MemoryBank memory_;
};

}  // namespace neuro::routes::protagonist
