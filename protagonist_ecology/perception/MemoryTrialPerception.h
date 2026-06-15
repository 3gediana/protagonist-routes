#pragma once

#include "core/interfaces/IPerception.h"

namespace neuro::routes::protagonist {

class MemoryTrialPerception final : public IPerception {
public:
    static constexpr std::size_t kDim = 2;

    std::size_t dim() const override { return kDim; }
    void sense(const IAgent& agent, const IWorld& world, std::span<float> output) const override;
    const char* name() const override { return "memory_trial"; }
};

}  // namespace neuro::routes::protagonist
