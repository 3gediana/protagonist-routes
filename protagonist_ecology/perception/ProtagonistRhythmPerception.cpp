#include "perception/ProtagonistRhythmPerception.h"

#include "core/agent/Agent.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/SeasonLayer.h"
#include "core/world/layers/TimeOfDayLayer.h"

#include <algorithm>
#include <cmath>

namespace neuro::routes::protagonist {

namespace {
constexpr float kTwoPi = 6.283185307179586f;
// d(core_temp)/dt reported in degC/s and clamped to [-1, 1]. Thermal
// integration is on the order of <= ~0.3 degC/s (thermal_damage_per_second
// + fire warming), so this keeps the physiological trend inside range without
// saturating during normal play.
constexpr float kTrendClamp = 1.0f;
constexpr double kMinDt = 1e-6;
}  // namespace

void ProtagonistRhythmPerception::sense(const IAgent& self, const IWorld& world,
                                        std::span<float> out) const {
    if (out.size() < kDim) {
        return;
    }
    std::fill(out.begin(), out.end(), 0.0f);

    // --- cyclic day-night phase: sin/cos(2*pi * time_of_day) ---
    float time_of_day = 0.0f;
    if (const auto* tod = world.getLayer<TimeOfDayLayer>(); tod != nullptr) {
        time_of_day = tod->timeOfDay();
    }
    out[0] = std::sin(kTwoPi * time_of_day);
    out[1] = std::cos(kTwoPi * time_of_day);

    // --- cyclic annual phase: fold the discrete season index and the
    //     within-season progress into one continuous year fraction in [0,1),
    //     then sin/cos so winter->spring is smooth. ---
    float year_phase = 0.0f;
    if (const auto* season = world.getLayer<SeasonLayer>(); season != nullptr) {
        const float idx = static_cast<float>(static_cast<int>(season->currentSeason()));
        year_phase = (idx + season->seasonProgress()) / 4.0f;
    }
    out[2] = std::sin(kTwoPi * year_phase);
    out[3] = std::cos(kTwoPi * year_phase);

    // --- body-temperature trend d(core_temp)/dt (degC/s, clamped) ---
    // core_temperature lives on the concrete Agent (not the IAgent interface),
    // so recover it via dynamic_cast; fall back to the healthy set-point if the
    // self is not a concrete Agent.
    float cur_temp = Agent::kHealthyCoreTemperature;
    if (const auto* agent = dynamic_cast<const Agent*>(&self); agent != nullptr) {
        cur_temp = agent->coreTemperature();
    }
    const double now_s = world.currentTimeSeconds();

    float trend = last_trend_;
    if (!has_prev_) {
        has_prev_ = true;
        prev_temp_ = cur_temp;
        prev_time_s_ = now_s;
        trend = 0.0f;
        last_trend_ = 0.0f;
    } else {
        const double dt = now_s - prev_time_s_;
        // Only refresh on a real time advance. Repeated same-tick calls (e.g.
        // tracing) reuse the last computed trend instead of collapsing to 0 or
        // dividing by ~0.
        if (dt > kMinDt) {
            trend = std::clamp(static_cast<float>((cur_temp - prev_temp_) / dt),
                               -kTrendClamp, kTrendClamp);
            prev_temp_ = cur_temp;
            prev_time_s_ = now_s;
            last_trend_ = trend;
        }
    }
    out[4] = trend;
}

}  // namespace neuro::routes::protagonist
