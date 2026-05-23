#pragma once

// core/perception/WeatherPerception.h
//
// 6-dim one-hot perception of current weather. Brain receives a
// categorical signal per weather type so it can learn distinct
// behaviours for clear/cloudy/rain/storm/wind/typhoon conditions.
//
// Falls back to all-zero (clear weather) when WeatherLayer absent.

#include "core/interfaces/IPerception.h"
#include "core/world/layers/WeatherLayer.h"

namespace neuro {

class WeatherPerception final : public IPerception {
public:
    static constexpr std::size_t kDim = WeatherLayer::kWeatherCount;  // 6

    WeatherPerception() = default;

    std::size_t dim() const override { return kDim; }
    void sense(const IAgent& agent, const IWorld& world, std::span<float> output) const override;
    const char* name() const override { return "weather"; }
};

}  // namespace neuro
