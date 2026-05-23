#include "core/perception/WeatherPerception.h"

#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"

namespace neuro {

void WeatherPerception::sense(const IAgent& agent, const IWorld& world, std::span<float> output) const {
    (void)agent;
    if (output.size() < kDim) return;

    for (std::size_t i = 0; i < kDim; ++i) output[i] = 0.0f;

    const auto* weather = world.getLayer<WeatherLayer>();
    if (weather == nullptr) {
        output[static_cast<std::size_t>(WeatherType::Clear)] = 1.0f;
        return;
    }

    const auto t = weather->currentWeather();
    const auto idx = static_cast<std::size_t>(t);
    if (idx < kDim) output[idx] = 1.0f;
}

}  // namespace neuro
