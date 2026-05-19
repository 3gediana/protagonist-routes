#pragma once

#include <array>
#include <cstddef>
#include <random>
#include <vector>

namespace neuro {

// Lightweight genome for pathogen neural network.
// Encodes weights for a 3-input → 4-hidden → 3-output fixed topology.
// Total weights: (3*4 + 4) + (4*3 + 3) = 12+4 + 12+3 = 31
class PathogenGenome {
public:
    static constexpr std::size_t kInputDim = 3;
    static constexpr std::size_t kHiddenDim = 4;
    static constexpr std::size_t kOutputDim = 3;
    static constexpr std::size_t kWeightCount = (kInputDim * kHiddenDim + kHiddenDim) +
                                                 (kHiddenDim * kOutputDim + kOutputDim);

    // Antigenic strain signature: 4 floats representing surface proteins
    // Hosts gain immunity to specific strains; mutated strains can bypass immunity
    static constexpr std::size_t kStrainDim = 4;
    using StrainSignature = std::array<float, kStrainDim>;

    explicit PathogenGenome(std::uint32_t seed = 0);
    explicit PathogenGenome(std::vector<float> weights);

    const std::vector<float>& weights() const { return weights_; }
    std::vector<float>& weights() { return weights_; }

    const StrainSignature& strain() const { return strain_; }
    StrainSignature& strain() { return strain_; }

    // Create mutated copy of this genome
    PathogenGenome mutate(float perturb_rate = 0.8f,
                          float perturb_sigma = 0.3f,
                          float reset_rate = 0.05f,
                          std::uint32_t seed = 0) const;

    // Fitness tracking: how many successful transmissions
    std::size_t transmissionCount() const { return transmissions_; }
    void noteTransmission() { ++transmissions_; }

    // Generation tracking
    std::size_t generation() const { return generation_; }

private:
    std::vector<float> weights_;
    StrainSignature strain_ = {};
    std::size_t transmissions_ = 0;
    std::size_t generation_ = 0;
};

}  // namespace neuro
