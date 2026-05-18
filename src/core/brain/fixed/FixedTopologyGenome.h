#pragma once

#include "core/interfaces/IGenome.h"

#include <cstddef>
#include <memory>
#include <random>
#include <vector>

namespace neuro {

class FixedTopologyGenome final : public IGenome {
public:
    FixedTopologyGenome(GenomeId id,
                        std::size_t input_dim,
                        std::size_t hidden_dim,
                        std::size_t output_dim);

    GenomeId id() const override;
    std::unique_ptr<IGenome> clone() const override;
    std::size_t complexity() const override;

    void setId(GenomeId id);

    std::size_t inputDim() const;
    std::size_t hiddenDim() const;
    std::size_t outputDim() const;
    std::size_t parameterCount() const;

    const std::vector<float>& inputHiddenWeights() const;
    const std::vector<float>& hiddenBias() const;
    const std::vector<float>& hiddenOutputWeights() const;
    const std::vector<float>& outputBias() const;

    std::vector<float>& inputHiddenWeights();
    std::vector<float>& hiddenBias();
    std::vector<float>& hiddenOutputWeights();
    std::vector<float>& outputBias();

    static std::unique_ptr<FixedTopologyGenome> random(GenomeId id,
                                                       std::size_t input_dim,
                                                       std::size_t hidden_dim,
                                                       std::size_t output_dim,
                                                       std::mt19937& rng);

private:
    GenomeId id_;
    std::size_t input_dim_;
    std::size_t hidden_dim_;
    std::size_t output_dim_;
    std::vector<float> input_hidden_weights_;
    std::vector<float> hidden_bias_;
    std::vector<float> hidden_output_weights_;
    std::vector<float> output_bias_;
};

}  // namespace neuro
