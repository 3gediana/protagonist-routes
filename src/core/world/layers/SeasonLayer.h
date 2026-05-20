#pragma once

#include "core/interfaces/IWorldLayer.h"
#include "core/types/Aliases.h"
#include "core/types/Vec2.h"

#include <cstddef>

namespace neuro {

enum class SeasonType { Spring = 0, Summer = 1, Autumn = 2, Winter = 3 };

class SeasonLayer final : public IWorldLayer {
public:
    static constexpr const char* kName = "season";

    SeasonLayer(SimTimeSeconds season_length_seconds, SimTimeSeconds start_time_seconds = 0.0);

    const char* layerName() const override { return kName; }
    void onAttach(IWorld& world) override;
    void tick(IWorld& world, float dt) override;

    SeasonType currentSeason() const;
    float resourceFactor() const;
    float metabolismFactor() const;
    float seasonProgress() const;
    std::size_t seasonIndex() const;

    SimTimeSeconds seasonLengthSeconds() const { return season_length_seconds_; }

private:
    SimTimeSeconds season_length_seconds_;
    SimTimeSeconds elapsed_seconds_;
};

}  // namespace neuro
