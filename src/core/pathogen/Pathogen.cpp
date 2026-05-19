#include "core/pathogen/Pathogen.h"

#include <algorithm>
#include <cmath>

namespace neuro {

Pathogen::Pathogen(PathogenGenome genome)
    : genome_(std::move(genome)) {}

void Pathogen::think(float host_energy_norm, float host_immunity, float host_infection) {
    const float inputs[PathogenGenome::kInputDim] = {
        std::clamp(host_energy_norm, 0.0f, 1.0f),
        std::clamp(host_immunity, 0.0f, 1.0f),
        std::clamp(host_infection, 0.0f, 1.0f),
    };
    forward(inputs);

    // Map raw outputs to behavioral parameters
    // infection_speed: remap to [0.3, 1.2] — capped, can't be too aggressive
    infection_speed_ = 0.75f + 0.45f * outputs_[0];
    // spread_rate: remap to [0.5, 1.5] — moderate
    spread_rate_ = 1.0f + 0.5f * outputs_[1];
    // immune_evasion: remap to [0.0, 0.3] — limited
    immune_evasion_ = std::clamp(0.15f + 0.15f * outputs_[2], 0.0f, 0.3f);
}

void Pathogen::forward(const float inputs[PathogenGenome::kInputDim]) {
    const auto& w = genome_.weights();
    std::size_t wi = 0;

    // Input → Hidden
    for (std::size_t h = 0; h < PathogenGenome::kHiddenDim; ++h) {
        float sum = w[wi++];  // bias
        for (std::size_t i = 0; i < PathogenGenome::kInputDim; ++i) {
            sum += inputs[i] * w[wi++];
        }
        hidden_[h] = std::tanh(sum);
    }

    // Hidden → Output
    for (std::size_t o = 0; o < PathogenGenome::kOutputDim; ++o) {
        float sum = w[wi++];  // bias
        for (std::size_t h = 0; h < PathogenGenome::kHiddenDim; ++h) {
            sum += hidden_[h] * w[wi++];
        }
        outputs_[o] = std::tanh(sum);
        if (!std::isfinite(outputs_[o])) outputs_[o] = 0.0f;
    }
}

Pathogen Pathogen::offspring(std::uint32_t seed) const {
    auto child_genome = genome_.mutate(0.8f, 0.15f, 0.03f, seed);
    return Pathogen(std::move(child_genome));
}

void Pathogen::noteTransmission() {
    genome_.noteTransmission();
}

}  // namespace neuro
