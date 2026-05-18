#pragma once

#include "core/interfaces/IWorldLayer.h"
#include "core/types/Vec2.h"

namespace neuro {

class TimeOfDayLayer final : public IWorldLayer {
public:
    static constexpr const char* kName = "time_of_day";

    TimeOfDayLayer(float day_length_ticks, float initial_time = 0.0f);

    const char* layerName() const override { return kName; }
    void onAttach(IWorld& world) override;
    void tick(IWorld& world, float dt) override;

    float timeOfDay() const { return t_; }
    float lightLevel() const;
    bool isDay() const;
    bool isNight() const;

    float dayLength() const { return day_length_ticks_; }

private:
    float day_length_ticks_;
    float t_;
};

}  // namespace neuro
