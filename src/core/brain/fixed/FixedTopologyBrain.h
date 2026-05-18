#pragma once

#include "core/interfaces/IBrain.h"

#include <vector>

namespace neuro {

class FixedTopologyGenome;

class FixedTopologyBrain final : public IBrain {
public:
    explicit FixedTopologyBrain(const FixedTopologyGenome& genome);

    std::size_t inputDim() const override;
    std::size_t outputDim() const override;
    std::span<const float> forward(std::span<const float> inputs, float dt) override;
    void reset() override;
    std::size_t parameterCount() const override;

private:
    std::size_t input_dim_;
    std::size_t hidden_dim_;
    std::size_t output_dim_;
    std::vector<float> input_hidden_weights_;
    std::vector<float> hidden_bias_;
    std::vector<float> hidden_output_weights_;
    std::vector<float> output_bias_;
    std::vector<float> hidden_;
    std::vector<float> output_;
};

}  // namespace neuro
