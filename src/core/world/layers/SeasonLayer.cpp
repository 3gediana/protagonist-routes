#include "core/world/layers/SeasonLayer.h"

#include "core/interfaces/IWorld.h"

namespace neuro {

SeasonLayer::SeasonLayer(std::size_t ticks_per_season, std::size_t start_tick)
    : ticks_per_season_(ticks_per_season > 0 ? ticks_per_season : 150),
      tick_count_(start_tick) {}

void SeasonLayer::onAttach(IWorld& world) {
    (void)world;
}

void SeasonLayer::tick(IWorld& world, float dt) {
    (void)world;
    tick_count_ += static_cast<std::size_t>(dt);
}

SeasonType SeasonLayer::currentSeason() const {
    const std::size_t season_index = (tick_count_ / ticks_per_season_) % 4;
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
    return static_cast<float>(tick_count_ % ticks_per_season_) / static_cast<float>(ticks_per_season_);
}

}  // namespace neuro
