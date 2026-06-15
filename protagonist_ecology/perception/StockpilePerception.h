#pragma once

#include "core/interfaces/IPerception.h"

namespace neuro::routes::protagonist {

class StockpilePerception final : public IPerception {
public:
    static constexpr std::size_t kDim = 4;

    explicit StockpilePerception(float normalization_scale = 12.0f) : normalization_scale_(normalization_scale) {}

    std::size_t dim() const override { return kDim; }
    void sense(const IAgent& self, const IWorld& world, std::span<float> out) const override;
    const char* name() const override { return "stockpile"; }

private:
    float normalization_scale_;
};

}  // namespace neuro::routes::protagonist
