#pragma once

#include "core/interfaces/IPerception.h"

namespace neuro::routes::protagonist {

class TreePerception final : public IPerception {
public:
    static constexpr std::size_t kDim = 5;

    TreePerception(float sense_radius, float chop_radius)
        : sense_radius_(sense_radius), chop_radius_(chop_radius) {}

    std::size_t dim() const override { return kDim; }
    void sense(const IAgent& self, const IWorld& world, std::span<float> out) const override;
    const char* name() const override { return "tree"; }

private:
    float sense_radius_;
    float chop_radius_;
};

}  // namespace neuro::routes::protagonist
