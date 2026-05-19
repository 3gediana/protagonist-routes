#include "core/pathogen/PathogenGenome.h"

#include <algorithm>
#include <cmath>

namespace neuro {

PathogenGenome::PathogenGenome(std::uint32_t seed)
    : weights_(kWeightCount, 0.0f) {
    std::mt19937 rng(seed == 0 ? std::random_device{}() : seed);
    std::normal_distribution<float> dist(0.0f, 0.3f);
    for (auto& w : weights_) {
        w = dist(rng);
    }
    // Random initial strain signature
    std::uniform_real_distribution<float> strain_dist(0.0f, 1.0f);
    for (auto& s : strain_) {
        s = strain_dist(rng);
    }
}

PathogenGenome::PathogenGenome(std::vector<float> weights)
    : weights_(std::move(weights)) {
    if (weights_.size() != kWeightCount) {
        weights_.resize(kWeightCount, 0.0f);
    }
}

PathogenGenome PathogenGenome::mutate(float perturb_rate,
                                       float perturb_sigma,
                                       float reset_rate,
                                       std::uint32_t seed) const {
    std::mt19937 rng(seed == 0 ? std::random_device{}() : seed);
    std::bernoulli_distribution do_perturb(perturb_rate);
    std::bernoulli_distribution do_reset(reset_rate);
    std::normal_distribution<float> perturb_dist(0.0f, perturb_sigma);
    std::uniform_real_distribution<float> reset_dist(-1.0f, 1.0f);

    auto child_weights = weights_;
    for (auto& w : child_weights) {
        if (do_reset(rng)) {
            w = reset_dist(rng);
        } else if (do_perturb(rng)) {
            w += perturb_dist(rng);
            if (!std::isfinite(w)) w = 0.0f;
        }
    }

    PathogenGenome child(std::move(child_weights));
    child.generation_ = generation_ + 1;

    // Antigenic drift: strain signature mutates with small perturbations
    std::normal_distribution<float> strain_perturb(0.0f, 0.1f);
    for (std::size_t i = 0; i < kStrainDim; ++i) {
        child.strain_[i] = std::clamp(strain_[i] + strain_perturb(rng), 0.0f, 1.0f);
    }

    return child;
}

}  // namespace neuro
