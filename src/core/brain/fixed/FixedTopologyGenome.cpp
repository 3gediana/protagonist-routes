#include "core/brain/fixed/FixedTopologyGenome.h"

#include <stdexcept>

namespace neuro {

namespace {

void fillRandom(std::vector<float>& data, std::mt19937& rng) {
    std::normal_distribution<float> dist(0.0f, 1.0f);
    for (float& value : data) {
        value = dist(rng);
    }
}

}  // namespace

FixedTopologyGenome::FixedTopologyGenome(GenomeId id,
                                         std::size_t input_dim,
                                         std::size_t hidden_dim,
                                         std::size_t output_dim)
    : id_(id),
      input_dim_(input_dim),
      hidden_dim_(hidden_dim),
      output_dim_(output_dim),
      input_hidden_weights_(input_dim * hidden_dim, 0.0f),
      hidden_bias_(hidden_dim, 0.0f),
      hidden_output_weights_(hidden_dim * output_dim, 0.0f),
      output_bias_(output_dim, 0.0f) {
    if (input_dim_ == 0 || hidden_dim_ == 0 || output_dim_ == 0) {
        throw std::invalid_argument("FixedTopologyGenome dims must be > 0");
    }
}

GenomeId FixedTopologyGenome::id() const {
    return id_;
}

std::unique_ptr<IGenome> FixedTopologyGenome::clone() const {
    return std::make_unique<FixedTopologyGenome>(*this);
}

std::size_t FixedTopologyGenome::complexity() const {
    return parameterCount();
}

void FixedTopologyGenome::setId(GenomeId id) {
    id_ = id;
}

std::size_t FixedTopologyGenome::inputDim() const {
    return input_dim_;
}

std::size_t FixedTopologyGenome::hiddenDim() const {
    return hidden_dim_;
}

std::size_t FixedTopologyGenome::outputDim() const {
    return output_dim_;
}

std::size_t FixedTopologyGenome::parameterCount() const {
    return input_hidden_weights_.size()
         + hidden_bias_.size()
         + hidden_output_weights_.size()
         + output_bias_.size();
}

const std::vector<float>& FixedTopologyGenome::inputHiddenWeights() const {
    return input_hidden_weights_;
}

const std::vector<float>& FixedTopologyGenome::hiddenBias() const {
    return hidden_bias_;
}

const std::vector<float>& FixedTopologyGenome::hiddenOutputWeights() const {
    return hidden_output_weights_;
}

const std::vector<float>& FixedTopologyGenome::outputBias() const {
    return output_bias_;
}

std::vector<float>& FixedTopologyGenome::inputHiddenWeights() {
    return input_hidden_weights_;
}

std::vector<float>& FixedTopologyGenome::hiddenBias() {
    return hidden_bias_;
}

std::vector<float>& FixedTopologyGenome::hiddenOutputWeights() {
    return hidden_output_weights_;
}

std::vector<float>& FixedTopologyGenome::outputBias() {
    return output_bias_;
}

std::unique_ptr<FixedTopologyGenome> FixedTopologyGenome::random(GenomeId id,
                                                                 std::size_t input_dim,
                                                                 std::size_t hidden_dim,
                                                                 std::size_t output_dim,
                                                                 std::mt19937& rng) {
    auto genome = std::make_unique<FixedTopologyGenome>(id, input_dim, hidden_dim, output_dim);
    fillRandom(genome->input_hidden_weights_, rng);
    fillRandom(genome->hidden_bias_, rng);
    fillRandom(genome->hidden_output_weights_, rng);
    fillRandom(genome->output_bias_, rng);
    return genome;
}

}  // namespace neuro
