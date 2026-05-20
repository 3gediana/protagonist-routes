#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace neuro::routes::protagonist {

class MemoryBank {
public:
    explicit MemoryBank(std::size_t cell_count);

    std::size_t cellCount() const { return values_.size(); }
    std::span<const float> values() const { return values_; }

    void reset();
    void update(std::span<const float> candidates,
                std::span<const float> write_gates,
                float decay);

private:
    std::vector<float> values_;
};

}  // namespace neuro::routes::protagonist
