#pragma once

#include "brain/DNCMemory.h"
#include "core/interfaces/IGenome.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <random>
#include <vector>

namespace neuro::routes::protagonist {

class ProtagonistGenome final : public IGenome {
public:
    ProtagonistGenome(GenomeId id,
                      std::size_t input_dim,
                      std::size_t hidden_dim,
                      std::size_t memory_cells,
                      std::size_t memory_slots,
                      std::size_t read_heads,
                      std::size_t action_dim);

    GenomeId id() const override;
    std::unique_ptr<IGenome> clone() const override;
    std::size_t complexity() const override;

    void setId(GenomeId id);

    std::size_t inputDim() const { return input_dim_; }
    std::size_t hiddenDim() const { return hidden_dim_; }
    std::size_t memoryCells() const { return memory_cells_; }
    std::size_t memorySlots() const { return memory_slots_; }
    std::size_t readHeads() const { return read_heads_; }
    std::size_t actionDim() const { return action_dim_; }
    std::size_t interfaceDim() const { return DNCMemory::interfaceDim(memory_cells_, read_heads_); }
    std::size_t parameterCount() const;

    const std::vector<float>& inputHiddenWeights() const { return input_hidden_weights_; }
    const std::vector<float>& hiddenBias() const { return hidden_bias_; }
    const std::vector<float>& hiddenActionWeights() const { return hidden_action_weights_; }
    const std::vector<float>& actionBias() const { return action_bias_; }
    const std::vector<float>& hiddenInterfaceWeights() const { return hidden_interface_weights_; }
    const std::vector<float>& interfaceBias() const { return interface_bias_; }

    std::vector<float>& inputHiddenWeights() { return input_hidden_weights_; }
    std::vector<float>& hiddenBias() { return hidden_bias_; }
    std::vector<float>& hiddenActionWeights() { return hidden_action_weights_; }
    std::vector<float>& actionBias() { return action_bias_; }
    std::vector<float>& hiddenInterfaceWeights() { return hidden_interface_weights_; }
    std::vector<float>& interfaceBias() { return interface_bias_; }

    static std::unique_ptr<ProtagonistGenome> random(GenomeId id,
                                                     std::size_t input_dim,
                                                     std::size_t hidden_dim,
                                                     std::size_t memory_cells,
                                                     std::size_t memory_slots,
                                                     std::size_t read_heads,
                                                     std::size_t action_dim,
                                                     std::mt19937& rng);

    bool saveToFile(const std::filesystem::path& path) const;
    static std::unique_ptr<ProtagonistGenome> loadFromFile(const std::filesystem::path& path);

private:
    GenomeId id_;
    std::size_t input_dim_;
    std::size_t hidden_dim_;
    std::size_t memory_cells_;
    std::size_t memory_slots_;
    std::size_t read_heads_;
    std::size_t action_dim_;
    std::vector<float> input_hidden_weights_;
    std::vector<float> hidden_bias_;
    std::vector<float> hidden_action_weights_;
    std::vector<float> action_bias_;
    std::vector<float> hidden_interface_weights_;
    std::vector<float> interface_bias_;
};

}  // namespace neuro::routes::protagonist
