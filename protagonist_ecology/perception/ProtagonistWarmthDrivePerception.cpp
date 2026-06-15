#include "perception/ProtagonistWarmthDrivePerception.h"

#include "core/agent/Agent.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/TimeOfDayLayer.h"

#include <algorithm>
#include <cmath>

namespace neuro::routes::protagonist {

namespace {
// d(core_temp)/dt reported in degC/s and clamped to [-1, 1]; thermal
// integration is on the order of <= ~0.3 degC/s in normal play.
constexpr float kTrendClamp = 1.0f;
constexpr double kMinDt = 1e-6;
// Look ahead a small fraction of a day so the warmth need *leads* the cold
// trough rather than tracking it in-phase. Falls back to a fixed horizon when
// there is no day-night layer.
constexpr double kHorizonFracOfDay = 0.05;
constexpr double kFallbackHorizonS = 30.0;
// Set-point span used to normalise the homeostatic deficit into [0, 1].
constexpr float kSpan = Agent::kHealthyCoreTemperature - Agent::kMinCoreTemperature;
}  // namespace

void ProtagonistWarmthDrivePerception::sense(const IAgent& self, const IWorld& world,
                                             std::span<float> out) const {
    if (out.size() < kDim) {
        return;
    }
    std::fill(out.begin(), out.end(), 0.0f);

    // --- current body core temperature ---
    // core_temperature lives on the concrete Agent (not the IAgent interface),
    // so recover it via dynamic_cast; fall back to the healthy set-point if the
    // self is not a concrete Agent.
    float cur_temp = Agent::kHealthyCoreTemperature;
    if (const auto* agent = dynamic_cast<const Agent*>(&self); agent != nullptr) {
        cur_temp = agent->coreTemperature();
    }
    const double now_s = world.currentTimeSeconds();

    // --- body-temperature trend d(core_temp)/dt (degC/s, clamped) ---
    // Only refresh on a real time advance. Repeated same-tick calls (e.g.
    // tracing) reuse the last computed trend instead of collapsing to 0 or
    // dividing by ~0.
    float trend = last_trend_;
    if (!has_prev_) {
        has_prev_ = true;
        prev_temp_ = cur_temp;
        prev_time_s_ = now_s;
        trend = 0.0f;
        last_trend_ = 0.0f;
    } else {
        const double dt = now_s - prev_time_s_;
        if (dt > kMinDt) {
            trend = std::clamp(static_cast<float>((cur_temp - prev_temp_) / dt),
                               -kTrendClamp, kTrendClamp);
            prev_temp_ = cur_temp;
            prev_time_s_ = now_s;
            last_trend_ = trend;
        }
    }

    // --- proactive homeostatic warmth need ---
    // Extrapolate the temperature trend a short horizon ahead, then express the
    // gap to the healthy set-point as a need in [0, 1]. A cooling trend
    // (trend < 0) pushes the predicted temperature down and raises the need
    // *before* the agent actually gets cold.
    double horizon_s = kFallbackHorizonS;
    if (const auto* tod = world.getLayer<TimeOfDayLayer>(); tod != nullptr) {
        const double day_len = static_cast<double>(tod->dayLength());
        if (day_len > 0.0) {
            horizon_s = day_len * kHorizonFracOfDay;
        }
    }

    const float predicted = cur_temp + static_cast<float>(horizon_s) * trend;
    const float warmth_need =
        std::clamp((Agent::kHealthyCoreTemperature - predicted) / kSpan, 0.0f, 1.0f);

    out[0] = warmth_need;
    out[1] = trend;
}

}  // namespace neuro::routes::protagonist
