#pragma once

#include "brain/DNCMemory.h"
#include "brain/SpatialMemoryBank.h"
#include "brain/EpisodicMemoryBank.h"
#include "brain/SocialMemoryBank.h"
#include "brain/GoalMemory.h"
#include "core/interfaces/IBrain.h"

#include <memory>
#include <vector>

namespace neuro::routes::protagonist {

struct DNCStateSummary {
    float usage_mean{0.0f};
    float usage_max{0.0f};
    float write_peak{0.0f};
    float read_peak{0.0f};
    float read_norm{0.0f};
    float precedence_peak{0.0f};
    float write_focus_slot{0.0f};
    float read_focus_slot{0.0f};
};

class ProtagonistGenome;
class ProtagonistDenseGpu;

class ProtagonistBrain final : public IBrain {
public:
    ProtagonistBrain(std::size_t input_dim,
                     std::size_t hidden_dim,
                     std::size_t memory_cells,
                     std::size_t memory_slots,
                     std::size_t read_heads,
                     std::size_t action_dim,
                     std::uint32_t seed,
                     bool enable_gpu_dense = false);
    explicit ProtagonistBrain(const ProtagonistGenome& genome, bool enable_gpu_dense = false);
    ~ProtagonistBrain() override;

    std::size_t inputDim() const override;
    std::size_t outputDim() const override;
    std::span<const float> forward(std::span<const float> inputs, float dt) override;
    void reset() override;
    std::size_t parameterCount() const override;

    std::size_t hiddenDim() const { return hidden_dim_; }
    std::size_t memoryCells() const { return memory_cells_; }
    std::size_t memorySlots() const { return memory_slots_; }
    std::size_t readHeads() const { return read_heads_; }

    DNCStateSummary dncSummary() const;

    DNCMemory& dncMemory() { return dnc_memory_; }
    const DNCMemory& dncMemory() const { return dnc_memory_; }

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
    std::size_t memory_cells_;
    std::size_t memory_slots_;
    std::size_t read_heads_;
    std::size_t action_dim_;
    std::vector<float> in_hidden_weights_;
    std::vector<float> hidden_bias_;
    std::vector<float> hidden_action_weights_;
    std::vector<float> action_bias_;
    std::vector<float> hidden_interface_weights_;
    std::vector<float> interface_bias_;
    std::vector<float> hidden_;
    std::vector<float> outputs_;
    std::unique_ptr<ProtagonistDenseGpu> dense_gpu_;
    DNCMemory dnc_memory_;

    SpatialMemoryBank spatial_memory_;
    EpisodicMemoryBank episodic_memory_;
    SocialMemoryBank social_memory_;
    GoalMemory goal_memory_;
};

}  // namespace neuro::routes::protagonist
