#pragma once

#include "core/interfaces/IPerception.h"

namespace neuro::routes::protagonist {

// Candidate-2 "homeostatic warmth-drive" (decision_round_10). Gives the
// protagonist an internal DRIVE (not a reward, not raw world perception): a
// warmth need that rises as its body core temperature falls below the healthy
// set-point, made PROACTIVE by extrapolating the current d(core_temp)/dt a
// short horizon ahead so the need climbs *before* the cold trough actually
// bites:
//   [0] warmth_need        clamp((healthy - predicted_core_temp)/(healthy-min), 0, 1)
//                          predicted_core_temp = core_temp + horizon * d(core_temp)/dt
//   [1] d(core_temp)/dt    body-temperature trend (degC/s, clamped)
//
// Rationale (Keramati & Gutkin 2014, homeostatic reinforcement learning): a
// drive defined as the deviation from a set-point naturally induces
// anticipatory action to prevent a *future* homeostatic deficit. The selection
// pressure is unchanged (cold still only kills through survival, fitness is
// untouched); this merely hands the policy an internal goal it can act on ahead
// of time. candidate-1 (extra perception) showed the agent already has enough
// information -- what was missing was a drive to use it for an early move.
//
// Enabling this adds kDim input dims, which changes the protagonist genome
// input_dim, so it starts a FRESH lineage (gated by
// protagonist_warmth_drive_enabled, default false). It is INDEPENDENT of
// candidate-1 (protagonist_rhythm_perception_enabled); per the "one minimal
// mechanism at a time" red line the two are NOT stacked.
class ProtagonistWarmthDrivePerception final : public IPerception {
public:
    static constexpr std::size_t kDim = 2;

    std::size_t dim() const override { return kDim; }
    void sense(const IAgent& self, const IWorld& world, std::span<float> out) const override;
    const char* name() const override { return "protagonist_warmth_drive"; }

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
