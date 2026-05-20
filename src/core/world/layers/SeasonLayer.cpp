#include "core/world/layers/SeasonLayer.h"

#include "core/interfaces/IWorld.h"

#include <algorithm>
#include <cmath>

namespace neuro {

SeasonLayer::SeasonLayer(SimTimeSeconds season_length_seconds, SimTimeSeconds start_time_seconds)
    : season_length_seconds_(season_length_seconds > 0.0 ? season_length_seconds : 150.0),
      elapsed_seconds_(std::max<SimTimeSeconds>(0.0, start_time_seconds)) {}

void SeasonLayer::onAttach(IWorld& world) {
    (void)world;
}

void SeasonLayer::tick(IWorld& world, float dt) {
    (void)world;
    elapsed_seconds_ += static_cast<SimTimeSeconds>(std::max(0.0f, dt));
}

SeasonType SeasonLayer::currentSeason() const {
    const std::size_t season_index = seasonIndex();
    return static_cast<SeasonType>(season_index);
}

float SeasonLayer::resourceFactor() const {
    switch (currentSeason()) {
        case SeasonType::Spring:  return 1.0f;
        case SeasonType::Summer:  return 1.5f;
        case SeasonType::Autumn:  return 0.8f;
        case SeasonType::Winter:  return 0.3f;
    }
    return 1.0f;
}

float SeasonLayer::metabolismFactor() const {
    switch (currentSeason()) {
        case SeasonType::Spring:  return 1.0f;
        case SeasonType::Summer:  return 0.9f;
        case SeasonType::Autumn:  return 1.1f;
        case SeasonType::Winter:  return 1.5f;
    }
    return 1.0f;
}

float SeasonLayer::seasonProgress() const {
    const auto phase = std::fmod(elapsed_seconds_, season_length_seconds_);
    return static_cast<float>(phase / season_length_seconds_);
}

std::size_t SeasonLayer::seasonIndex() const {
    return static_cast<std::size_t>(elapsed_seconds_ / season_length_seconds_) % 4;
}

}  // namespace neuro
