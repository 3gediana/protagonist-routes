#pragma once

#include <cstdint>

namespace dp::world {

// Stage W1 (decision_blueprint_world_coherence, user mandate "下雨要能浇灭火，
// 这些机制要相互完善"): a single GLOBAL weather state machine that drives the
// flagship coupling chain
//
//     rain → douses exposed fire → no warmth → night cold → the agent must
//     re-light / shelter / wear clothes; rain also wets the agent (faster
//     cold drain) and refills natural / farm water.
//
// The weather state is global (one sky over the whole world); the per-LOCATION
// bias "沼泽多雨 / 沙漠几乎不雨" is applied by the caller (Environment) at the
// point of contact via a biome gate, so this class stays tiny and unit-
// testable in isolation.
//
// DEFAULT DISABLED. With enabled=false the sky is permanently clear
// (intensity 0, raining()==false) so old-world checkpoints / runs are
// byte-identical to the pre-weather world.
class WeatherSystem {
public:
    struct Config {
        bool  enabled          = false;   // master switch; off => always clear
        // Chance PER MINUTE that a clear sky turns to rain. 0.30 => roughly
        // one shower every few minutes, so a 5-min episode usually sees rain.
        float rain_prob_per_min = 0.30f;
        // Mean rain duration in seconds (a shower lasts ~this long).
        float rain_dur_s        = 90.0f;
        // Seconds for intensity to ramp 0<->1 when the state flips (a shower
        // builds up / dies down instead of snapping on, so douse / wetness are
        // continuous and the policy can react to the onset).
        float ramp_s            = 8.0f;
    };

    explicit WeatherSystem(const Config& cfg = {}, uint64_t seed = 0x5EED5)
        : cfg_(cfg), rng_(seed ? seed : 0x5EED5) {}

    // Advance the sky one frame. No-op (stays clear) when disabled.
    void tick(float dt);

    // True while a shower is active (state == Rain). False when disabled.
    bool raining() const { return raining_; }

    // Current rain strength in [0,1] (ramped). 0 when clear / disabled.
    float intensity() const { return intensity_; }

    // Episode reset: re-seed deterministically and return to a clear sky.
    void reset(uint64_t seed);

    // Set the config after default construction (Environment plumbs CLI knobs
    // here in its ctor body to avoid member-init-order churn).
    void configure(const Config& cfg) { cfg_ = cfg; }

    const Config& config() const { return cfg_; }

private:
    float frand01() {
        rng_ ^= rng_ << 13;
        rng_ ^= rng_ >> 7;
        rng_ ^= rng_ << 17;
        return float(rng_ >> 11) * (1.0f / 9007199254740992.0f);  // [0,1)
    }

    Config   cfg_;
    uint64_t rng_;
    bool     raining_    = false;
    float    intensity_  = 0.0f;
    float    rain_timer_ = 0.0f;   // seconds left in the current shower
};

}  // namespace dp::world
