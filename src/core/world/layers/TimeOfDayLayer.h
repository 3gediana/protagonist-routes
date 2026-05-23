#pragma once

#include "core/interfaces/IWorldLayer.h"
#include "core/types/Aliases.h"
#include "core/types/Vec2.h"

namespace neuro {

// Tracks a continuous phase t_ ∈ [0, 1) within a day-night cycle.
//
// Convention (used throughout: TimeOfDayPerception, MoveCapability,
// PredatorHuntCapability, FleeCapability, SignalPerception,
// HabitatReward, InfectionPerception, ComprehensiveMetrics):
//   t = 0.00  ─  midnight
//   t = 0.25  ─  sunrise
//   t = 0.50  ─  noon
//   t = 0.75  ─  sunset
//
// Layer 1 (Final Blueprint) added several derived quantities for new
// systems (ThermalTickCapability, perception extensions) while keeping
// the original timeOfDay() / lightLevel() / isDay() / isNight()
// outputs bit-identical for backward compatibility with v17 trained
// networks.
class TimeOfDayLayer final : public IWorldLayer {
public:
    static constexpr const char* kName = "time_of_day";

    TimeOfDayLayer(SimTimeSeconds day_length_seconds, float initial_time = 0.0f);

    const char* layerName() const override { return kName; }
    void onAttach(IWorld& world) override;
    void tick(IWorld& world, float dt) override;

    // ----- Original API (preserved, bit-identical to pre-Layer-1) -----

    float timeOfDay() const { return t_; }
    float lightLevel() const;
    bool isDay() const;
    bool isNight() const;

    SimTimeSeconds dayLength() const { return day_length_seconds_; }

    // ----- Layer 1 additions (Final Blueprint, 2026-05-23) -----

    // sin(2π × phase): smooth daily wave.
    //   t=0.00 (midnight) : 0
    //   t=0.25 (sunrise)  : +1
    //   t=0.50 (noon)     : 0
    //   t=0.75 (sunset)   : -1
    // Smooth across the t=0/1 boundary (unlike raw timeOfDay()).
    float phaseSin() const;

    // cos(2π × phase): orthogonal wave. Combined with phaseSin
    // uniquely encodes t ∈ [0, 1) without discontinuity.
    //   t=0.00 (midnight) : +1
    //   t=0.25 (sunrise)  : 0
    //   t=0.50 (noon)     : -1
    //   t=0.75 (sunset)   : 0
    float phaseCos() const;

    // Visibility multiplier for perception ranges and vision sensors:
    //   full day      (0.25-0.75) : 1.0
    //   twilight      (0.20-0.25, 0.75-0.80) : 0.5 → 1.0 lerp
    //   full night    (0.80-1.0, 0-0.20) : 0.5
    float visibilityFactor() const;

    // Ambient temperature delta vs day baseline (°C). Used by
    // ThermalTickCapability (Layer 1) to bias core_temperature_
    // away from setpoint at night and toward setpoint at noon.
    //   noon       : +4°C  (warmest moment of day)
    //   midnight   : -8°C  (coldest, asymmetric: nights cool faster)
    float ambientTemperatureDelta() const;

private:
    SimTimeSeconds day_length_seconds_;
    float t_;
};

}  // namespace neuro
