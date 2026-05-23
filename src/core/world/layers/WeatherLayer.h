#pragma once

// core/world/layers/WeatherLayer.h
//
// FINAL_BLUEPRINT.md Layer 2: weather system. Maintains a current
// WeatherType + a transition schedule.
//
// 6 weather types per FINAL_BLUEPRINT.md §2.3:
//   Clear   (default, 70%)
//   Cloudy  (15%)
//   Rain    (10%)             -> hydration auto-recovery (per-tick)
//   Storm   (3%, summer-biased)-> rain + 5% lightning per minute per tree
//   HighWind (1.5%)            -> move cost x1.5, signal diffusion x2
//   Typhoon  (0.5%, summer only)-> wind + rain + tree fall probability
//
// Weather state evolves stochastically: each weather has a sampled
// duration; on expiry, next weather is sampled by base probabilities
// optionally biased by SeasonLayer (summer biases storm/typhoon).
//
// Convention:
//   - All durations measured in SimTimeSeconds.
//   - This layer is purely a state-holder + scheduler. Effects on
//     other layers (hydration recovery, move cost, lightning fires)
//     live in those layers' tick() loops, which query
//     world.getLayer<WeatherLayer>() for current state.

#include "core/interfaces/IWorldLayer.h"
#include "core/types/Aliases.h"

#include <cstdint>
#include <random>

namespace neuro {

enum class WeatherType : int {
    Clear     = 0,
    Cloudy    = 1,
    Rain      = 2,
    Storm     = 3,
    HighWind  = 4,
    Typhoon   = 5,
    Count     = 6,
};

const char* weatherTypeName(WeatherType type);

class WeatherLayer final : public IWorldLayer {
public:
    static constexpr const char* kName = "weather";
    static constexpr std::size_t kWeatherCount = static_cast<std::size_t>(WeatherType::Count);

    struct Coefficients {
        // Average duration of a weather state in simulation seconds.
        // Default 600s = 10 sim minutes; weather doesn't flip rapidly.
        SimTimeSeconds average_duration_seconds = 600.0;
        SimTimeSeconds min_duration_seconds = 120.0;
        SimTimeSeconds max_duration_seconds = 1200.0;

        // Base transition probabilities (sum should be ~1.0).
        // Index = WeatherType int value.
        // From FINAL_BLUEPRINT.md §2.3.
        float base_prob_clear    = 0.70f;
        float base_prob_cloudy   = 0.15f;
        float base_prob_rain     = 0.10f;
        float base_prob_storm    = 0.03f;
        float base_prob_highwind = 0.015f;
        float base_prob_typhoon  = 0.005f;

        // Season bias multipliers. When SeasonLayer is present, the
        // weather sampler multiplies base probs by these factors.
        // Per FINAL_BLUEPRINT.md §2.3 ("夏季雷暴更频繁") and §2.4.
        // Summer: storms x3, typhoons x4 (typhoon only summer).
        // Winter: rain x0.5 (snow approximation - no separate snow yet).
        float summer_storm_mult   = 3.0f;
        float summer_typhoon_mult = 4.0f;
        float winter_typhoon_mult = 0.0f;   // no typhoons in winter
        float winter_rain_mult    = 0.5f;
    };

    explicit WeatherLayer(Coefficients coeffs = {}, std::uint32_t seed = 0,
                          WeatherType initial = WeatherType::Clear);

    const char* layerName() const override { return kName; }
    void onAttach(IWorld& world) override;
    void tick(IWorld& world, float dt) override;

    WeatherType currentWeather() const { return current_; }
    SimTimeSeconds timeInState() const { return time_in_state_; }
    SimTimeSeconds plannedDuration() const { return planned_duration_; }

    // Convenience predicates for downstream layers.
    bool isRaining() const {
        return current_ == WeatherType::Rain
            || current_ == WeatherType::Storm
            || current_ == WeatherType::Typhoon;
    }
    bool isStorming() const {
        return current_ == WeatherType::Storm || current_ == WeatherType::Typhoon;
    }
    bool isWindy() const {
        return current_ == WeatherType::HighWind || current_ == WeatherType::Typhoon;
    }

    // Movement cost multiplier (1.0 default, 1.5 in wind, 1.8 in typhoon).
    float moveCostMultiplier() const;

    // Hydration recovery rate per second (0 default, +0.5 rain, +0.8 storm/typhoon).
    float hydrationRecoveryPerSecond() const;

    // Signal diffusion multiplier (1.0 default, 2.0 wind/typhoon).
    float signalDiffusionMultiplier() const;

    // Lightning probability per tree per second (0 default,
    // small in storm, larger in typhoon).
    float lightningPerTreePerSecond() const;

    // Per-tree-per-second probability of being felled by typhoon
    // (0 outside typhoon).
    float treeFellProbabilityPerSecond() const;

private:
    WeatherType sampleNextWeather(IWorld& world);
    SimTimeSeconds sampleDuration();

    Coefficients coeffs_;
    std::mt19937 rng_;
    WeatherType current_;
    SimTimeSeconds time_in_state_;
    SimTimeSeconds planned_duration_;
};

}  // namespace neuro
