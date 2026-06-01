#pragma once

// Layer 1 (Final Blueprint) thermal regulation tick capability.
//
// Updates Agent.core_temperature_ each tick by integrating:
//   - ambient temperature from SeasonLayer (season baseline °C)
//   - day-night cycle from TimeOfDayLayer.ambientTemperatureDelta()
//   - campfire warming from CampfireLayer.inWarmthZone()
//   - metabolic heat from |velocity| (movement produces heat)
//   - radiative loss proportional to (core - ambient)
//
// Convention:
//   - All temperatures in °C.
//   - Healthy setpoint = 37.0 °C (Agent::kHealthyCoreTemperature).
//   - SurvivalStatusLayer reads coreTemperature() to apply
//     severity-graded debuffs (Layer 1 B1.3).
//
// Designed to gracefully degrade when ancillary layers absent:
//   - No SeasonLayer    → fixed mild ambient (18 °C, spring baseline)
//   - No TimeOfDayLayer → no day-night delta
//   - No CampfireLayer  → no warming bonus
//
// Passive: consumedOutputs() == 0. Cheap: O(1) per agent if no
// CampfireLayer query, O(N_fires) otherwise.

#include "core/interfaces/IAgentCapability.h"

#include <cstddef>

namespace neuro::routes::protagonist {

class ThermalTickCapability final : public IAgentCapability {
public:
    struct Coefficients {
        // Season-specific ambient temperature baselines (°C).
        float ambient_spring = 18.0f;
        float ambient_summer = 28.0f;
        float ambient_autumn = 15.0f;
        float ambient_winter = 5.0f;

        // Per-second °C gain from sustained movement, scaled by |velocity|.
        // Default: at 1 m/s, +0.1 °C/s metabolic heat (small).
        float metabolic_heat_per_speed_per_sec = 0.1f;

        // Per-second °C gain when inside campfire warmth zone.
        // Strong enough to offset most cold-night losses near a fire.
        float fire_warming_per_sec = 1.0f;

        // Exposure loss coefficient: dT/dt = -k × (T - ambient).
        // Default 0.05 / s gives ~20s half-life toward ambient, which
        // is fast enough that overnight cooling is visible but slow
        // enough that minor speed changes don't immediately matter.
        float radiative_loss_per_sec = 0.05f;

        // Optional clamp range. Should be wider than healthy band so
        // that severe debuffs can play out in SurvivalStatusLayer.
        float min_core_temperature = 20.0f;
        float max_core_temperature = 50.0f;
    };

    explicit ThermalTickCapability(Coefficients coeffs = {})
        : coeffs_(coeffs) {}

    std::size_t consumedOutputs() const override { return 0; }
    void apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) override;
    const char* name() const override { return "thermal_tick"; }

private:
    Coefficients coeffs_;
};

}  // namespace neuro::routes::protagonist
