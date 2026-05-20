#pragma once

#include "core/interfaces/IPerception.h"

namespace neuro::routes::protagonist {

class WorksitePerception final : public IPerception {
public:
    static constexpr std::size_t kDim = 8;

    explicit WorksitePerception(float sense_radius) : sense_radius_(sense_radius) {}

    std::size_t dim() const override { return kDim; }
    void sense(const IAgent& self, const IWorld& world, std::span<float> out) const override;
    const char* name() const override { return "worksite"; }

private:
    float sense_radius_;
};

}  // namespace neuro::routes::protagonist
