#include "brain/DNCMemory.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace neuro::routes::protagonist {

namespace {

constexpr float kEpsilon = 1.0e-6f;

float sigmoid(float x) {
    if (x >= 0.0f) {
        const float z = std::exp(-x);
        return 1.0f / (1.0f + z);
    }
    const float z = std::exp(x);
    return z / (1.0f + z);
}

float softplus(float x) {
    if (x > 20.0f) {
        return x;
    }
    if (x < -20.0f) {
        return std::exp(x);
    }
    return std::log1p(std::exp(x));
}

void softmaxInPlace(std::vector<float>& values) {
    if (values.empty()) {
        return;
    }
    const float max_value = *std::max_element(values.begin(), values.end());
    float sum = 0.0f;
    for (float& value : values) {
        value = std::exp(value - max_value);
        sum += value;
    }
    if (sum <= kEpsilon) {
        const float uniform = 1.0f / static_cast<float>(values.size());
        std::fill(values.begin(), values.end(), uniform);
        return;
    }
    for (float& value : values) {
        value /= sum;
    }
}

float dotProduct(const float* lhs, const float* rhs, std::size_t count) {
    float out = 0.0f;
    for (std::size_t i = 0; i < count; ++i) {
        out += lhs[i] * rhs[i];
    }
    return out;
}

float vectorNorm(const float* values, std::size_t count) {
    return std::sqrt(std::max(dotProduct(values, values, count), 0.0f));
}

}  // namespace

DNCMemory::DNCMemory(std::size_t slot_count, std::size_t word_size, std::size_t read_head_count)
    : slot_count_(slot_count > 0 ? slot_count : 64),
      word_size_(word_size > 0 ? word_size : 32),
      read_head_count_(read_head_count > 0 ? read_head_count : 4),
      memory_(slot_count_ * word_size_, 0.0f),
      usage_(slot_count_, 0.0f),
      precedence_(slot_count_, 0.0f),
      temporal_link_(slot_count_ * slot_count_, 0.0f),
      write_weighting_(slot_count_, 0.0f),
      read_weightings_(read_head_count_ * slot_count_, 0.0f),
      read_vectors_(read_head_count_ * word_size_, 0.0f) {}

std::size_t DNCMemory::interfaceDim(std::size_t word_size, std::size_t read_head_count) {
    return word_size
        + 1
        + word_size
        + word_size
        + read_head_count
        + 1
        + 1
        + read_head_count * word_size
        + read_head_count
        + read_head_count * 3;
}

void DNCMemory::reset() {
    std::fill(memory_.begin(), memory_.end(), 0.0f);
    std::fill(usage_.begin(), usage_.end(), 0.0f);
    std::fill(precedence_.begin(), precedence_.end(), 0.0f);
    std::fill(temporal_link_.begin(), temporal_link_.end(), 0.0f);
    std::fill(write_weighting_.begin(), write_weighting_.end(), 0.0f);
    std::fill(read_weightings_.begin(), read_weightings_.end(), 0.0f);
    std::fill(read_vectors_.begin(), read_vectors_.end(), 0.0f);
}

std::vector<float> DNCMemory::contentAddress(const std::vector<float>& key, float strength) const {
    std::vector<float> scores(slot_count_, 0.0f);
    const float key_norm = vectorNorm(key.data(), word_size_);
    const float positive_strength = 1.0f + softplus(strength);
    for (std::size_t slot = 0; slot < slot_count_; ++slot) {
        const float* memory_row = memory_.data() + slot * word_size_;
        const float mem_norm = vectorNorm(memory_row, word_size_);
        const float denom = std::max(key_norm * mem_norm, kEpsilon);
        scores[slot] = positive_strength * (dotProduct(key.data(), memory_row, word_size_) / denom);
    }
    softmaxInPlace(scores);
    return scores;
}

std::vector<float> DNCMemory::allocationWeighting() const {
    std::vector<std::size_t> indices(slot_count_, 0);
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&](std::size_t lhs, std::size_t rhs) {
        return usage_[lhs] < usage_[rhs];
    });

    std::vector<float> allocation(slot_count_, 0.0f);
    float prod = 1.0f;
    for (const std::size_t idx : indices) {
        allocation[idx] = (1.0f - usage_[idx]) * prod;
        prod *= usage_[idx];
    }
    return allocation;
}

std::vector<float> DNCMemory::directionalWeighting(std::size_t head_index, bool forward) const {
    std::vector<float> weighting(slot_count_, 0.0f);
    const float* prev_read = read_weightings_.data() + head_index * slot_count_;
    for (std::size_t to = 0; to < slot_count_; ++to) {
        float sum = 0.0f;
        for (std::size_t from = 0; from < slot_count_; ++from) {
            const float link = forward
                ? temporal_link_[from * slot_count_ + to]
                : temporal_link_[to * slot_count_ + from];
            sum += link * prev_read[from];
        }
        weighting[to] = sum;
    }
    return weighting;
}

void DNCMemory::updateUsage(const std::vector<float>& free_gates) {
    std::vector<float> retention(slot_count_, 1.0f);
    for (std::size_t head = 0; head < read_head_count_; ++head) {
        const float free_gate = head < free_gates.size() ? std::clamp(free_gates[head], 0.0f, 1.0f) : 0.0f;
        const float* read_weight = read_weightings_.data() + head * slot_count_;
        for (std::size_t slot = 0; slot < slot_count_; ++slot) {
            retention[slot] *= (1.0f - free_gate * read_weight[slot]);
        }
    }

    for (std::size_t slot = 0; slot < slot_count_; ++slot) {
        const float used = usage_[slot] + write_weighting_[slot] - usage_[slot] * write_weighting_[slot];
        usage_[slot] = std::clamp(used * retention[slot], 0.0f, 1.0f);
    }
}

void DNCMemory::writeMemory(const std::vector<float>& write_weighting,
                            const std::vector<float>& erase_vector,
                            const std::vector<float>& write_vector) {
    for (std::size_t slot = 0; slot < slot_count_; ++slot) {
        const float weight = write_weighting[slot];
        float* row = memory_.data() + slot * word_size_;
        for (std::size_t col = 0; col < word_size_; ++col) {
            row[col] = row[col] * (1.0f - weight * erase_vector[col]) + weight * write_vector[col];
        }
    }
}

void DNCMemory::updateTemporalLinks(const std::vector<float>& write_weighting) {
    const float write_sum = std::accumulate(write_weighting.begin(), write_weighting.end(), 0.0f);
    std::vector<float> new_link(slot_count_ * slot_count_, 0.0f);
    for (std::size_t i = 0; i < slot_count_; ++i) {
        for (std::size_t j = 0; j < slot_count_; ++j) {
            if (i == j) {
                new_link[i * slot_count_ + j] = 0.0f;
                continue;
            }
            const float prev_link = temporal_link_[i * slot_count_ + j];
            const float reset_factor = 1.0f - write_weighting[i] - write_weighting[j];
            new_link[i * slot_count_ + j] = reset_factor * prev_link + write_weighting[i] * precedence_[j];
        }
    }
    temporal_link_.swap(new_link);

    std::vector<float> next_precedence(slot_count_, 0.0f);
    for (std::size_t slot = 0; slot < slot_count_; ++slot) {
        next_precedence[slot] = (1.0f - write_sum) * precedence_[slot] + write_weighting[slot];
    }
    precedence_.swap(next_precedence);
}

void DNCMemory::updateReadHeads(const std::vector<float>& read_keys,
                                const std::vector<float>& read_strengths,
                                const std::vector<float>& read_modes) {
    std::vector<float> next_read_weightings(read_weightings_.size(), 0.0f);
    std::vector<float> next_read_vectors(read_vectors_.size(), 0.0f);

    for (std::size_t head = 0; head < read_head_count_; ++head) {
        const float* key_ptr = read_keys.data() + head * word_size_;
        std::vector<float> key(key_ptr, key_ptr + word_size_);
        auto content = contentAddress(key, read_strengths[head]);
        auto backward = directionalWeighting(head, false);
        auto forward = directionalWeighting(head, true);

        const float backward_mode = read_modes[head * 3 + 0];
        const float content_mode = read_modes[head * 3 + 1];
        const float forward_mode = read_modes[head * 3 + 2];
        float* read_weight = next_read_weightings.data() + head * slot_count_;
        float* read_vector = next_read_vectors.data() + head * word_size_;

        for (std::size_t slot = 0; slot < slot_count_; ++slot) {
            read_weight[slot] = backward_mode * backward[slot]
                              + content_mode * content[slot]
                              + forward_mode * forward[slot];
        }

        for (std::size_t col = 0; col < word_size_; ++col) {
            float value = 0.0f;
            for (std::size_t slot = 0; slot < slot_count_; ++slot) {
                value += read_weight[slot] * memory_[slot * word_size_ + col];
            }
            read_vector[col] = value;
        }
    }

    read_weightings_.swap(next_read_weightings);
    read_vectors_.swap(next_read_vectors);
}

void DNCMemory::step(const DNCMemoryInterface& interface_state) {
    if (interface_state.write_key.size() != word_size_
        || interface_state.erase_vector.size() != word_size_
        || interface_state.write_vector.size() != word_size_
        || interface_state.free_gates.size() != read_head_count_
        || interface_state.read_keys.size() != read_head_count_ * word_size_
        || interface_state.read_strengths.size() != read_head_count_
        || interface_state.read_modes.size() != read_head_count_ * 3) {
        return;
    }

    updateUsage(interface_state.free_gates);

    auto allocation = allocationWeighting();
    auto write_content = contentAddress(interface_state.write_key, interface_state.write_strength);
    std::vector<float> next_write_weighting(slot_count_, 0.0f);
    const float allocation_gate = std::clamp(interface_state.allocation_gate, 0.0f, 1.0f);
    const float write_gate = std::clamp(interface_state.write_gate, 0.0f, 1.0f);
    for (std::size_t slot = 0; slot < slot_count_; ++slot) {
        next_write_weighting[slot] = write_gate * (allocation_gate * allocation[slot] + (1.0f - allocation_gate) * write_content[slot]);
    }

    writeMemory(next_write_weighting, interface_state.erase_vector, interface_state.write_vector);
    updateTemporalLinks(next_write_weighting);
    write_weighting_.swap(next_write_weighting);

    updateReadHeads(interface_state.read_keys, interface_state.read_strengths, interface_state.read_modes);
}

}  // namespace neuro::routes::protagonist
