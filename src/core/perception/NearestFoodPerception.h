#pragma once

#include "core/interfaces/IPerception.h"

namespace neuro {

class NearestFoodPerception final : public IPerception {
public:
    NearestFoodPerception() = default;

    std::size_t dim() const override;
    void sense(const IAgent& self, const IWorld& world, std::span<float> out) const override;
    const char* name() const override;
};

}  // namespace neuro
