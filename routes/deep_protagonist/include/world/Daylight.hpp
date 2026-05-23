#pragma once

#include <raylib.h>

namespace dp::world {

// A simple day/night cycle. Time-of-day t is normalized to [0, 1):
//   0.00  midnight
//   0.25  sunrise
//   0.50  noon
//   0.75  sunset
//   1.00  back to midnight
//
// One in-game day defaults to 600 real seconds (=10 minutes) so the user
// can see a full cycle without waiting forever. Pure cosmetic in S2; S3
// will use this signal to schedule wolf nighttime aggression etc.
class Daylight {
public:
    explicit Daylight(float seconds_per_day = 600.0f, float start_t = 0.30f);

    void advance(float dt);

    float t() const { return t_; }                  // [0,1)
    float seconds_per_day() const { return spd_; }
    float sun_height_normalized() const;            // [-1, 1], +1 = noon
    bool  is_day() const { return sun_height_normalized() > 0.0f; }

    Color sky_color() const;                        // for ClearBackground
    Color terrain_tint() const;                     // multiply terrain colors

private:
    float spd_;     // seconds per in-game day
    float t_;       // current normalized time
};

}  // namespace dp::world
