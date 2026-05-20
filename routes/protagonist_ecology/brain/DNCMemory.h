#pragma once

#include <cstddef>
#include <vector>

namespace neuro::routes::protagonist {

struct DNCMemoryInterface {
    std::vector<float> write_key;
    float write_strength{0.0f};
    std::vector<float> erase_vector;
    std::vector<float> write_vector;
    std::vector<float> free_gates;
    float allocation_gate{0.0f};
    float write_gate{0.0f};
    std::vector<float> read_keys;
    std::vector<float> read_strengths;
    std::vector<float> read_modes;
};

class DNCMemory {
public:
    DNCMemory(std::size_t slot_count, std::size_t word_size, std::size_t read_head_count);

    static std::size_t interfaceDim(std::size_t word_size, std::size_t read_head_count);

    void reset();
    void step(const DNCMemoryInterface& interface_state);

    std::size_t slotCount() const { return slot_count_; }
    std::size_t wordSize() const { return word_size_; }
    std::size_t readHeadCount() const { return read_head_count_; }
    std::size_t readVectorDim() const { return read_vectors_.size(); }

    const std::vector<float>& memory() const { return memory_; }
    const std::vector<float>& usage() const { return usage_; }
    const std::vector<float>& precedence() const { return precedence_; }
    const std::vector<float>& temporalLink() const { return temporal_link_; }
    const std::vector<float>& writeWeighting() const { return write_weighting_; }
    const std::vector<float>& readWeightings() const { return read_weightings_; }
    const std::vector<float>& readVectors() const { return read_vectors_; }

private:
    std::vector<float> contentAddress(const std::vector<float>& key, float strength) const;
    std::vector<float> allocationWeighting() const;
    std::vector<float> directionalWeighting(std::size_t head_index, bool forward) const;
    void updateUsage(const std::vector<float>& free_gates);
    void writeMemory(const std::vector<float>& write_weighting,
                     const std::vector<float>& erase_vector,
                     const std::vector<float>& write_vector);
    void updateTemporalLinks(const std::vector<float>& write_weighting);
    void updateReadHeads(const std::vector<float>& read_keys,
                         const std::vector<float>& read_strengths,
                         const std::vector<float>& read_modes);

    std::size_t slot_count_;
    std::size_t word_size_;
    std::size_t read_head_count_;
    std::vector<float> memory_;
    std::vector<float> usage_;
    std::vector<float> precedence_;
    std::vector<float> temporal_link_;
    std::vector<float> write_weighting_;
    std::vector<float> read_weightings_;
    std::vector<float> read_vectors_;
};

}  // namespace neuro::routes::protagonist
