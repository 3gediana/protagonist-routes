#include "core/world/layers/TimeOfDayLayer.h"

#include "core/interfaces/IWorld.h"

#include <cmath>

namespace neuro {

TimeOfDayLayer::TimeOfDayLayer(float day_length_ticks, float initial_time)
    : day_length_ticks_(day_length_ticks > 0.0f ? day_length_ticks : 600.0f),
      t_(initial_time) {}

void TimeOfDayLayer::onAttach(IWorld& world) {
    (void)world;
}

void TimeOfDayLayer::tick(IWorld& world, float dt) {
    (void)world;
    t_ += dt / day_length_ticks_;
    if (t_ >= 1.0f) {
        t_ -= 1.0f;
    }
}

float TimeOfDayLayer::lightLevel() const {
    return 0.5f + 0.5f * std::sin(t_ * 2.0f * 3.14159265f - 1.5708f);
}

bool TimeOfDayLayer::isDay() const {
    return lightLevel() > 0.5f;
}

bool TimeOfDayLayer::isNight() const {
    return lightLevel() <= 0.5f;
}

}  // namespace neuro
