#include "core/world/layers/WeatherLayer.h"

#include "core/interfaces/IWorld.h"
#include "core/world/layers/SeasonLayer.h"

#include <algorithm>
#include <array>

namespace neuro {

namespace {
constexpr float kEps = 1e-6f;
}  // namespace

const char* weatherTypeName(WeatherType type) {
    switch (type) {
        case WeatherType::Clear:    return "clear";
        case WeatherType::Cloudy:   return "cloudy";
        case WeatherType::Rain:     return "rain";
        case WeatherType::Storm:    return "storm";
        case WeatherType::HighWind: return "high_wind";
        case WeatherType::Typhoon:  return "typhoon";
        default:                    return "unknown";
    }
}

WeatherLayer::WeatherLayer(Coefficients coeffs, std::uint32_t seed, WeatherType initial)
    : coeffs_(coeffs),
      rng_(seed == 0 ? 123456789u : seed),
      current_(initial),
      time_in_state_(0.0),
      planned_duration_(coeffs_.average_duration_seconds) {}

void WeatherLayer::onAttach(IWorld& world) {
    (void)world;
    planned_duration_ = sampleDuration();
}

void WeatherLayer::tick(IWorld& world, float dt) {
    if (dt <= 0.0f) return;
    time_in_state_ += static_cast<SimTimeSeconds>(dt);
    if (time_in_state_ >= planned_duration_) {
        current_ = sampleNextWeather(world);
        time_in_state_ = 0.0;
        planned_duration_ = sampleDuration();
    }
}

SimTimeSeconds WeatherLayer::sampleDuration() {
    // Truncated exponential: mean = average_duration, clamp to [min, max].
    std::exponential_distribution<double> exp(1.0 / std::max(1.0, coeffs_.average_duration_seconds));
    double sample = exp(rng_);
    sample = std::clamp(sample, coeffs_.min_duration_seconds, coeffs_.max_duration_seconds);
    return static_cast<SimTimeSeconds>(sample);
}

WeatherType WeatherLayer::sampleNextWeather(IWorld& world) {
    // Build per-type weight by base prob * season bias.
    std::array<float, kWeatherCount> w{};
    w[static_cast<int>(WeatherType::Clear)]    = coeffs_.base_prob_clear;
    w[static_cast<int>(WeatherType::Cloudy)]   = coeffs_.base_prob_cloudy;
    w[static_cast<int>(WeatherType::Rain)]     = coeffs_.base_prob_rain;
    w[static_cast<int>(WeatherType::Storm)]    = coeffs_.base_prob_storm;
    w[static_cast<int>(WeatherType::HighWind)] = coeffs_.base_prob_highwind;
    w[static_cast<int>(WeatherType::Typhoon)]  = coeffs_.base_prob_typhoon;

    if (const auto* season = world.getLayer<SeasonLayer>(); season != nullptr) {
        const SeasonType s = season->currentSeason();
        if (s == SeasonType::Summer) {
            w[static_cast<int>(WeatherType::Storm)]   *= coeffs_.summer_storm_mult;
            w[static_cast<int>(WeatherType::Typhoon)] *= coeffs_.summer_typhoon_mult;
        } else if (s == SeasonType::Winter) {
            w[static_cast<int>(WeatherType::Typhoon)] *= coeffs_.winter_typhoon_mult;
            w[static_cast<int>(WeatherType::Rain)]    *= coeffs_.winter_rain_mult;
        }
    }

    // Avoid being stuck in same weather: zero out current weight (forces transition).
    w[static_cast<int>(current_)] = 0.0f;

    // Normalize.
    float sum = 0.0f;
    for (auto v : w) sum += v;
    if (sum < kEps) {
        return WeatherType::Clear;  // fallback
    }

    std::uniform_real_distribution<float> u(0.0f, sum);
    float roll = u(rng_);
    float cumul = 0.0f;
    for (std::size_t i = 0; i < kWeatherCount; ++i) {
        cumul += w[i];
        if (roll < cumul) {
            return static_cast<WeatherType>(i);
        }
    }
    return WeatherType::Clear;
}

float WeatherLayer::moveCostMultiplier() const {
    switch (current_) {
        case WeatherType::HighWind: return 1.5f;
        case WeatherType::Typhoon:  return 1.8f;
        default:                    return 1.0f;
    }
}

float WeatherLayer::hydrationRecoveryPerSecond() const {
    switch (current_) {
        case WeatherType::Rain:     return 0.5f;
        case WeatherType::Storm:    return 0.8f;
        case WeatherType::Typhoon:  return 1.0f;
        default:                    return 0.0f;
    }
}

float WeatherLayer::signalDiffusionMultiplier() const {
    switch (current_) {
        case WeatherType::HighWind: return 2.0f;
        case WeatherType::Typhoon:  return 2.5f;
        default:                    return 1.0f;
    }
}

float WeatherLayer::lightningPerTreePerSecond() const {
    switch (current_) {
        case WeatherType::Storm:   return 0.0001f;   // ~0.36% per tree per minute
        case WeatherType::Typhoon: return 0.0003f;   // ~1% per tree per minute
        default:                   return 0.0f;
    }
}

float WeatherLayer::treeFellProbabilityPerSecond() const {
    return current_ == WeatherType::Typhoon ? 0.00005f : 0.0f;
}

}  // namespace neuro
