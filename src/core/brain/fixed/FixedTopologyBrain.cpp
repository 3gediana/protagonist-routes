#include "core/brain/fixed/FixedTopologyBrain.h"

#include "core/brain/fixed/FixedTopologyGenome.h"

#include <cmath>
#include <stdexcept>

namespace neuro {

namespace {

float activate(float x) {
    return std::tanh(x);
}

}  // namespace

FixedTopologyBrain::FixedTopologyBrain(const FixedTopologyGenome& genome)
    : input_dim_(genome.inputDim()),
      hidden_dim_(genome.hiddenDim()),
      output_dim_(genome.outputDim()),
      input_hidden_weights_(genome.inputHiddenWeights()),
      hidden_bias_(genome.hiddenBias()),
      hidden_output_weights_(genome.hiddenOutputWeights()),
      output_bias_(genome.outputBias()),
      hidden_(hidden_dim_, 0.0f),
      output_(output_dim_, 0.0f) {}

std::size_t FixedTopologyBrain::inputDim() const {
    return input_dim_;
}

std::size_t FixedTopologyBrain::outputDim() const {
    return output_dim_;
}

std::span<const float> FixedTopologyBrain::forward(std::span<const float> inputs, float) {
    if (inputs.size() != input_dim_) {
        throw std::invalid_argument("FixedTopologyBrain input dim mismatch");
    }

    for (std::size_t h = 0; h < hidden_dim_; ++h) {
        float sum = hidden_bias_[h];
        for (std::size_t i = 0; i < input_dim_; ++i) {
            sum += inputs[i] * input_hidden_weights_[i * hidden_dim_ + h];
        }
        hidden_[h] = activate(sum);
    }

    for (std::size_t o = 0; o < output_dim_; ++o) {
        float sum = output_bias_[o];
        for (std::size_t h = 0; h < hidden_dim_; ++h) {
            sum += hidden_[h] * hidden_output_weights_[h * output_dim_ + o];
        }
        output_[o] = activate(sum);
        if (!std::isfinite(output_[o])) output_[o] = 0.0f;
    }

    return output_;
}

void FixedTopologyBrain::reset() {
    std::fill(hidden_.begin(), hidden_.end(), 0.0f);
    std::fill(output_.begin(), output_.end(), 0.0f);
}

std::size_t FixedTopologyBrain::parameterCount() const {
    return input_hidden_weights_.size()
         + hidden_bias_.size()
         + hidden_output_weights_.size()
         + output_bias_.size();
}

}  // namespace neuro
