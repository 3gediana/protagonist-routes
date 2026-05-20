#include "core/perception/SignalPerception.h"

#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/signal/SignalLayer.h"
#include "core/world/layers/TimeOfDayLayer.h"
#include "core/world/layers/terrain/TerrainLayer.h"

#include <algorithm>
#include <cmath>

namespace neuro {

SignalPerception::SignalPerception(std::size_t channels, float perception_radius, bool terrain_modulation_enabled)
    : channels_(channels),
      perception_radius_(perception_radius),
      terrain_modulation_enabled_(terrain_modulation_enabled),
      previous_sample_(channels, 0.0f) {}

void SignalPerception::sense(const IAgent& agent, const IWorld& world, std::span<float> output) const {
    if (output.size() < dim()) return;

    const auto* signal_layer = world.getLayer<SignalLayer>();
    if (signal_layer == nullptr) {
        for (std::size_t i = 0; i < dim(); ++i) {
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

    // Terrain occlusion: caves block signals, high ground amplifies
    float terrain_factor = 1.0f;
    const auto* terrain = world.getLayer<TerrainLayer>();
    if (terrain_modulation_enabled_ && terrain) {
        const float h = terrain->heightAt(agent.position().x, agent.position().y);
        const float height_scale = std::max(terrain->heightScale(), 1.0f);
        if (terrain->inCave(agent.position().x, agent.position().y)) {
            terrain_factor = 0.2f;  // Cave: signals mostly blocked
        } else if (h > height_scale * 0.5f) {
            terrain_factor = 1.3f;  // High ground: better signal reception
        }
    }

    for (std::size_t i = 0; i < channels_ && i < sampled.size(); ++i) {
        const float current_value = std::tanh(sampled[i] * terrain_factor);
        const float previous_value = has_previous_sample_ ? previous_sample_[i] : current_value;
        output[i * kFeaturesPerChannel] = current_value;
        output[i * kFeaturesPerChannel + 1] = std::clamp(current_value - previous_value, -1.0f, 1.0f);
        previous_sample_[i] = current_value;
    }

    has_previous_sample_ = true;
}

}  // namespace neuro
