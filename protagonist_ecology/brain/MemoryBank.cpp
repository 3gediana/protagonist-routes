#include "brain/MemoryBank.h"

#include <algorithm>
#include <cmath>

namespace neuro::routes::protagonist {

MemoryBank::MemoryBank(std::size_t cell_count)
    : values_(cell_count, 0.0f) {}

void MemoryBank::reset() {
    std::fill(values_.begin(), values_.end(), 0.0f);
}

void MemoryBank::update(std::span<const float> candidates,
                        std::span<const float> write_gates,
                        float decay) {
    const float clamped_decay = std::clamp(decay, 0.0f, 1.0f);
    const std::size_t count = std::min(values_.size(), std::min(candidates.size(), write_gates.size()));
    for (std::size_t i = 0; i < count; ++i) {
        const float gate = std::clamp(write_gates[i], 0.0f, 1.0f);
        const float candidate = std::tanh(candidates[i]);
        const float retained = values_[i] * (1.0f - clamped_decay * (0.25f + 0.75f * gate));
        values_[i] = std::clamp(retained + gate * candidate, -1.0f, 1.0f);
    }
}

}  // namespace neuro::routes::protagonist
