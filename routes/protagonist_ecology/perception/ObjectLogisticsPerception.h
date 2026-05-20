#pragma once

#include "core/interfaces/IPerception.h"

namespace neuro::routes::protagonist {

class ObjectLogisticsPerception final : public IPerception {
public:
    static constexpr std::size_t kDim = 9;

    explicit ObjectLogisticsPerception(float sense_radius);

    std::size_t dim() const override { return kDim; }
    void sense(const IAgent& self, const IWorld& world, std::span<float> out) const override;
    const char* name() const override { return "object_logistics"; }

private:
    float sense_radius_;
};

}  // namespace neuro::routes::protagonist
