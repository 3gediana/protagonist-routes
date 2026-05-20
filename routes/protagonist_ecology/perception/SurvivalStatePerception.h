#pragma once

#include "core/interfaces/IPerception.h"

namespace neuro::routes::protagonist {

class SurvivalStatePerception final : public IPerception {
public:
    static constexpr std::size_t kDim = 6;

    std::size_t dim() const override { return kDim; }
    void sense(const IAgent& self, const IWorld& world, std::span<float> out) const override;
    const char* name() const override { return "survival_state"; }
};

}  // namespace neuro::routes::protagonist
