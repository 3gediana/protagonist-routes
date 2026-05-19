#pragma once

#include "core/pathogen/PathogenGenome.h"

#include <cmath>

namespace neuro {

// A pathogen entity attached to an infected host agent.
// Has a tiny NN that decides infection strategy based on host state.
//
// Inputs:  [host_energy_norm, host_immunity_progress, host_infection_progress]
// Outputs: [infection_speed, spread_rate, immune_evasion]
//
// infection_speed: multiplier on how fast infection_progress advances (>1 = aggressive)
// spread_rate: multiplier on transmission probability (>1 = more contagious)
// immune_evasion: reduces host immunity gain rate (0~1, higher = more evasion)
class Pathogen {
public:
    explicit Pathogen(PathogenGenome genome);

    // Run the pathogen NN and update behavioral parameters
    void think(float host_energy_norm, float host_immunity, float host_infection);

    // Behavioral outputs from the pathogen NN
    float infectionSpeed() const { return infection_speed_; }
    float spreadRate() const { return spread_rate_; }
    float immuneEvasion() const { return immune_evasion_; }

    // Genome access
    PathogenGenome& genome() { return genome_; }
    const PathogenGenome& genome() const { return genome_; }

    // Create a mutated offspring (for transmission)
    Pathogen offspring(std::uint32_t seed = 0) const;

    // Track successful transmission
    void noteTransmission();

private:
    void forward(const float inputs[PathogenGenome::kInputDim]);

    PathogenGenome genome_;
    float hidden_[PathogenGenome::kHiddenDim] = {};
    float outputs_[PathogenGenome::kOutputDim] = {};

    // Behavioral parameters (derived from NN outputs)
    float infection_speed_ = 1.0f;
    float spread_rate_ = 1.0f;
    float immune_evasion_ = 0.0f;
};

}  // namespace neuro
