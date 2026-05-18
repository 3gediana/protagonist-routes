#pragma once

#include "core/interfaces/IWorldLayer.h"
#include "core/types/Vec2.h"

#include <cstddef>

namespace neuro {

enum class SeasonType { Spring = 0, Summer = 1, Autumn = 2, Winter = 3 };

class SeasonLayer final : public IWorldLayer {
public:
    static constexpr const char* kName = "season";

    SeasonLayer(std::size_t ticks_per_season, std::size_t start_tick = 0);

    const char* layerName() const override { return kName; }
    void onAttach(IWorld& world) override;
    void tick(IWorld& world, float dt) override;

    SeasonType currentSeason() const;
    float resourceFactor() const;
    float metabolismFactor() const;
    float seasonProgress() const;

    std::size_t ticksPerSeason() const { return ticks_per_season_; }

private:
    std::size_t ticks_per_season_;
    std::size_t tick_count_;
};

}  // namespace neuro
