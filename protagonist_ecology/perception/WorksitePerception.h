#pragma once

#include "core/interfaces/IPerception.h"

namespace neuro::routes::protagonist {

class WorksitePerception final : public IPerception {
public:
    static constexpr std::size_t kDim = 8;

    explicit WorksitePerception(float sense_radius, bool require_visibility = false)
        : sense_radius_(sense_radius), require_visibility_(require_visibility) {}

    std::size_t dim() const override { return kDim; }
    void sense(const IAgent& self, const IWorld& world, std::span<float> out) const override;
    const char* name() const override { return "worksite"; }

private:
    float sense_radius_;
    bool require_visibility_;
};

}  // namespace neuro::routes::protagonist
