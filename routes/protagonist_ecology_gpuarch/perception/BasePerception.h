#pragma once

#include "core/interfaces/IPerception.h"

namespace neuro::routes::protagonist {

class BasePerception final : public IPerception {
public:
    static constexpr std::size_t kDim = 10;

    BasePerception() = default;

    std::size_t dim() const override { return kDim; }
    void sense(const IAgent& self, const IWorld& world, std::span<float> out) const override;
    const char* name() const override { return "base"; }
};

}  // namespace neuro::routes::protagonist
