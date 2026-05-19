#include "core/perception/SignalPerception.h"

#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/signal/SignalLayer.h"
#include "core/world/layers/TimeOfDayLayer.h"

namespace neuro {

SignalPerception::SignalPerception(std::size_t channels, float perception_radius)
    : channels_(channels), perception_radius_(perception_radius) {}

void SignalPerception::sense(const IAgent& agent, const IWorld& world, std::span<float> output) const {
    if (output.size() < channels_) return;

    const auto* signal_layer = world.getLayer<SignalLayer>();
    if (signal_layer == nullptr) {
        for (std::size_t i = 0; i < channels_; ++i) {
            output[i] = 0.0f;
        }
        return;
    }

    // Day/night: darkness reduces perception radius (night = 0.5x range)
    float effective_radius = perception_radius_;
    const auto* tod = world.getLayer<TimeOfDayLayer>();
    if (tod != nullptr) {
        const float light = tod->lightLevel();
        effective_radius *= (0.5f + 0.5f * light);
    }

    const auto sampled = signal_layer->sample(agent.position(), effective_radius);
    for (std::size_t i = 0; i < channels_ && i < sampled.size(); ++i) {
        output[i] = sampled[i];
    }
}

}  // namespace neuro
