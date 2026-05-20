#pragma once

#include "core/interfaces/IPerception.h"

namespace neuro::routes::protagonist {

class MemoryCognitionPerception final : public IPerception {
public:
    static constexpr std::size_t kDim = 44;

    MemoryCognitionPerception();

    std::size_t dim() const override { return kDim; }
    void sense(const IAgent& agent, const IWorld& world, std::span<float> output) const override;
    const char* name() const override { return "memory_cognition"; }
};

}  // namespace neuro::routes::protagonist
