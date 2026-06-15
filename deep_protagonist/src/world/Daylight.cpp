#include "world/Daylight.hpp"

#include <algorithm>
#include <cmath>

namespace dp::world {

namespace {
constexpr float kTwoPi = 6.2831853f;

inline unsigned char clampb(float x) {
    if (x < 0.0f)   return 0;
    if (x > 255.0f) return 255;
    return static_cast<unsigned char>(x);
}

inline Color lerp_color(Color a, Color b, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return Color{
        clampb(a.r + (b.r - a.r) * t),
        clampb(a.g + (b.g - a.g) * t),
        clampb(a.b + (b.b - a.b) * t),
        255
    };
}
}  // namespace

Daylight::Daylight(float seconds_per_day, float start_t)
    : spd_(seconds_per_day > 1.0f ? seconds_per_day : 1.0f),
      t_(start_t) {
    while (t_ < 0.0f) t_ += 1.0f;
    while (t_ >= 1.0f) t_ -= 1.0f;
}

void Daylight::advance(float dt) {
    t_ += dt / spd_;
    while (t_ >= 1.0f) t_ -= 1.0f;
}

float Daylight::sun_height_normalized() const {
    // sun height = sin(2pi * (t - 0.25))   so noon=+1, midnight=-1
    return std::sin(kTwoPi * (t_ - 0.25f));
}

Color Daylight::sky_color() const {
    // Five anchor colours across the day:
    //   night  (dark blue) -> dawn (pink) -> day (light blue) -> dusk (orange) -> night
    Color night = Color{ 10, 12, 30, 255 };
    Color dawn  = Color{ 230, 150, 130, 255 };
    Color day   = Color{ 135, 180, 220, 255 };
    Color dusk  = Color{ 240, 130, 60, 255 };

    // Map t into a 4-segment piecewise lerp
    if (t_ < 0.20f) {                       // deep night -> dawn building
        float u = t_ / 0.20f;
        return lerp_color(night, dawn, u);
    } else if (t_ < 0.30f) {                // dawn -> day
        float u = (t_ - 0.20f) / 0.10f;
        return lerp_color(dawn, day, u);
    } else if (t_ < 0.70f) {                // full day
        return day;
    } else if (t_ < 0.80f) {                // day -> dusk
        float u = (t_ - 0.70f) / 0.10f;
        return lerp_color(day, dusk, u);
    } else if (t_ < 0.90f) {                // dusk -> night
        float u = (t_ - 0.80f) / 0.10f;
        return lerp_color(dusk, night, u);
    } else {                                 // night
        return night;
    }
}

Color Daylight::terrain_tint() const {
    // Tint multiplier: bright at noon, dim at night, warm at dusk
    float h = sun_height_normalized();   // [-1, 1]
    // brightness = max(0.25, 0.55 + 0.45 * h)  -> 0.25 at midnight, 1.0 at noon
    float b = std::max(0.25f, 0.55f + 0.45f * h);
    // Slight warm tint at sunrise/sunset
    bool warm = (t_ > 0.18f && t_ < 0.32f) || (t_ > 0.68f && t_ < 0.82f);
    float r_mul = b * (warm ? 1.05f : 1.0f);
    float g_mul = b * (warm ? 0.92f : 1.0f);
    float b_mul = b * (warm ? 0.80f : 1.0f);
    return Color{
        clampb(255.0f * r_mul),
        clampb(255.0f * g_mul),
        clampb(255.0f * b_mul),
        255
    };
}

}  // namespace dp::world
