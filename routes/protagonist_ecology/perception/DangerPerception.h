#pragma once

#include "core/interfaces/IPerception.h"
#include "perception/FovGate.h"

namespace neuro::routes::protagonist {

class DangerPerception final : public IPerception {
public:
    static constexpr std::size_t kDim = 5;

    explicit DangerPerception(float sense_radius, FovParams fov = {})
        : sense_radius_(sense_radius), fov_(fov) {}

    std::size_t dim() const override { return kDim; }
    void sense(const IAgent& self, const IWorld& world, std::span<float> out) const override;
    const char* name() const override { return "danger"; }

private:
    float sense_radius_;
    FovParams fov_;
};

}  // namespace neuro::routes::protagonist
