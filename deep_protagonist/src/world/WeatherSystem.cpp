#include "world/WeatherSystem.hpp"

#include <algorithm>

namespace dp::world {

void WeatherSystem::tick(float dt) {
    if (!cfg_.enabled) {
        // Permanently clear: keep the old world byte-identical.
        raining_    = false;
        intensity_  = 0.0f;
        rain_timer_ = 0.0f;
        return;
    }

    const float dt_min = dt / 60.0f;

    if (raining_) {
        rain_timer_ -= dt;
        if (rain_timer_ <= 0.0f) {
            raining_    = false;
            rain_timer_ = 0.0f;
        }
    } else {
        // Bernoulli per tick at the configured per-minute rate.
        float p_start = cfg_.rain_prob_per_min * dt_min;
        if (frand01() < p_start) {
            raining_ = true;
            // Exponential-ish duration around the mean (0.5x..1.5x) so showers
            // aren't all identical length.
            rain_timer_ = cfg_.rain_dur_s * (0.5f + frand01());
        }
    }

    // Ramp intensity toward the target (1 while raining, 0 while clear) so the
    // shower fades in/out instead of snapping. ramp_s<=0 => instant.
    float target = raining_ ? 1.0f : 0.0f;
    if (cfg_.ramp_s <= 0.0f) {
        intensity_ = target;
    } else {
        float step = dt / cfg_.ramp_s;
        if (intensity_ < target) intensity_ = std::min(target, intensity_ + step);
        else if (intensity_ > target) intensity_ = std::max(target, intensity_ - step);
    }
}

void WeatherSystem::reset(uint64_t seed) {
    rng_        = seed ? seed : 0x5EED5;
    raining_    = false;
    intensity_  = 0.0f;
    rain_timer_ = 0.0f;
}

}  // namespace dp::world
