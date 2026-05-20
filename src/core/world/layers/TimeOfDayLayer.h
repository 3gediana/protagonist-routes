#pragma once

#include "core/interfaces/IWorldLayer.h"
#include "core/types/Aliases.h"
#include "core/types/Vec2.h"

namespace neuro {

class TimeOfDayLayer final : public IWorldLayer {
public:
    static constexpr const char* kName = "time_of_day";

    TimeOfDayLayer(SimTimeSeconds day_length_seconds, float initial_time = 0.0f);

    const char* layerName() const override { return kName; }
    void onAttach(IWorld& world) override;
    void tick(IWorld& world, float dt) override;

    float timeOfDay() const { return t_; }
    float lightLevel() const;
    bool isDay() const;
    bool isNight() const;

    SimTimeSeconds dayLength() const { return day_length_seconds_; }

private:
    SimTimeSeconds day_length_seconds_;
    float t_;
};

}  // namespace neuro
