#pragma once

#include "brain/MemoryBank.h"
#include "brain/SpatialMemoryBank.h"
#include "brain/EpisodicMemoryBank.h"
#include "brain/SocialMemoryBank.h"
#include "brain/GoalMemory.h"
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

    SpatialMemoryBank& spatialMemory() { return spatial_memory_; }
    const SpatialMemoryBank& spatialMemory() const { return spatial_memory_; }

    SocialMemoryBank& socialMemory() { return social_memory_; }
    const SocialMemoryBank& socialMemory() const { return social_memory_; }

    EpisodicMemoryBank& episodicMemory() { return episodic_memory_; }
    const EpisodicMemoryBank& episodicMemory() const { return episodic_memory_; }

    GoalMemory& goalMemory() { return goal_memory_; }
    const GoalMemory& goalMemory() const { return goal_memory_; }

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

    // Specialized Memory Banks V2
    SpatialMemoryBank spatial_memory_;
    EpisodicMemoryBank episodic_memory_;
    SocialMemoryBank social_memory_;
    GoalMemory goal_memory_;
};

}  // namespace neuro::routes::protagonist
