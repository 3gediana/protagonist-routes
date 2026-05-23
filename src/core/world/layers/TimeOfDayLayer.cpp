#include "core/world/layers/TimeOfDayLayer.h"

#include "core/interfaces/IWorld.h"

#include <algorithm>
#include <cmath>

namespace neuro {

namespace {
constexpr float kTwoPi = 6.2831853071795864769f;
}  // namespace

TimeOfDayLayer::TimeOfDayLayer(SimTimeSeconds day_length_seconds, float initial_time)
    : day_length_seconds_(day_length_seconds > 0.0 ? day_length_seconds : 600.0),
      t_(initial_time) {}

void TimeOfDayLayer::onAttach(IWorld& world) {
    (void)world;
}

void TimeOfDayLayer::tick(IWorld& world, float dt) {
    (void)world;
    t_ += static_cast<float>(static_cast<SimTimeSeconds>(dt) / day_length_seconds_);
    if (t_ >= 1.0f) {
        t_ -= 1.0f;
    }
}

// ----- Original API (bit-identical to pre-Layer-1) -----

float TimeOfDayLayer::lightLevel() const {
    return 0.5f + 0.5f * std::sin(t_ * 2.0f * 3.14159265f - 1.5708f);
}

bool TimeOfDayLayer::isDay() const {
    return lightLevel() > 0.5f;
}

bool TimeOfDayLayer::isNight() const {
    return lightLevel() <= 0.5f;
}

// ----- Layer 1 additions -----

float TimeOfDayLayer::phaseSin() const {
    return std::sin(kTwoPi * t_);
}

float TimeOfDayLayer::phaseCos() const {
    return std::cos(kTwoPi * t_);
}

float TimeOfDayLayer::visibilityFactor() const {
    if (t_ >= 0.25f && t_ < 0.75f) {
        return 1.0f;  // full day (06:00-18:00)
    }
    // Twilight bands [0.20, 0.25) and [0.75, 0.80) lerp 0.5 ↔ 1.0.
    if (t_ >= 0.20f && t_ < 0.25f) {
        return 0.5f + (t_ - 0.20f) / 0.05f * 0.5f;
    }
    if (t_ >= 0.75f && t_ < 0.80f) {
        return 1.0f - (t_ - 0.75f) / 0.05f * 0.5f;
    }
    return 0.5f;  // full night
}

float TimeOfDayLayer::ambientTemperatureDelta() const {
    // Asymmetric cosine: noon=+4°C, midnight=-8°C, sunrise/sunset=0.
    // c = -cos(2π × t) gives: t=0 → -1, t=0.25 → 0, t=0.5 → +1, t=0.75 → 0.
    const float c = -std::cos(kTwoPi * t_);
    if (c >= 0.0f) {
        return c * 4.0f;   // daytime warming up to +4°C
    } else {
        return c * 8.0f;   // nighttime cooling down to -8°C
    }
}

}  // namespace neuro
