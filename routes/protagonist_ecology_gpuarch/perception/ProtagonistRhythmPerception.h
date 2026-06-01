#pragma once

#include "core/interfaces/IPerception.h"

namespace neuro::routes::protagonist {

// Candidate-1 "sharpen perceptual encoding" (decision_round_9). Gives the
// protagonist a CONTINUOUS, circular encoding of its temporal context plus the
// trend of its own body temperature:
//   [0] sin(2*pi * time_of_day)      day-night phase (no 1->0 wrap discontinuity)
//   [1] cos(2*pi * time_of_day)
//   [2] sin(2*pi * year_phase)       annual phase (season index + within-season progress)
//   [3] cos(2*pi * year_phase)
//   [4] d(core_temp)/dt              body-temperature trend (degC/s, clamped)
//
// The raw TimeOfDayPerception / SeasonPerception expose linear scalars that wrap
// discontinuously (23:59 -> 00:00, winter -> spring); the sin/cos pairs remove
// that discontinuity so the brain can lock onto phase, and the temperature
// derivative gives an early "getting colder" signal ahead of the cold trough.
//
// Enabling this adds kDim input dims, which changes the protagonist genome
// input_dim, so it starts a FRESH lineage (gated by
// protagonist_rhythm_perception_enabled, default false -> existing checkpoints
// stay loadable).
class ProtagonistRhythmPerception final : public IPerception {
public:
    static constexpr std::size_t kDim = 5;

    std::size_t dim() const override { return kDim; }
    void sense(const IAgent& self, const IWorld& world, std::span<float> out) const override;
    const char* name() const override { return "protagonist_rhythm"; }

private:
    // Per-agent state for the body-temperature derivative. Each protagonist owns
    // its own perception instance (see createProtagonist), so this stays
    // deterministic under adt=1 replay.
    mutable bool has_prev_ = false;
    mutable float prev_temp_ = 0.0f;
    mutable double prev_time_s_ = 0.0;
    mutable float last_trend_ = 0.0f;
};

}  // namespace neuro::routes::protagonist
