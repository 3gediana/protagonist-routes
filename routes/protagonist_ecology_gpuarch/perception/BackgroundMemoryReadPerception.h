#pragma once

#include "core/interfaces/IPerception.h"

#include <cstddef>

namespace neuro::routes::protagonist {

// Layer 5 #9: emits the background agent's DNC read_vectors as
// perception input. Sized as `read_heads * word_size` floats. When
// the BackgroundMemoryLayer isn't present (feature disabled) this
// fills zeros, so a misconfigured toml degrades to "no memory" rather
// than crashing.
class BackgroundMemoryReadPerception final : public IPerception {
public:
    explicit BackgroundMemoryReadPerception(std::size_t read_vector_dim)
        : read_vector_dim_(read_vector_dim) {}

    std::size_t dim() const override { return read_vector_dim_; }
    void sense(const IAgent& self, const IWorld& world, std::span<float> out) const override;
    const char* name() const override { return "bg_memory_read"; }

private:
    std::size_t read_vector_dim_;
};

}  // namespace neuro::routes::protagonist
