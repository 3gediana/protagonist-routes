#pragma once

#include "core/interfaces/IPerception.h"

#include <cstddef>

namespace neuro {

// Food scent perception: provides rough direction and distance to nearest food.
// Unlike NearestFoodPerception which gives exact position, this gives a noisy
// gradient that's less precise but doesn't make communication redundant.
// When combined with SignalPerception, agents can use signals for precise
// navigation and scent for coarse direction — creating selection pressure
// for signal-following behavior.
class FoodScentPerception final : public IPerception {
public:
    static constexpr std::size_t kDim = 2;

    std::size_t dim() const override { return kDim; }
    void sense(const IAgent& agent, const IWorld& world, std::span<float> output) const override;
    const char* name() const override;
};

}  // namespace neuro
