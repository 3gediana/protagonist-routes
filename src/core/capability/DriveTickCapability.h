#pragma once

// L2.2 homeostatic drive state v2 (SPECIES_DIFFERENTIATION_SPEC.md § 2).
// Each tick, advances 4 of the 5 drive states on the owning agent:
//   hunger     : grows with metabolism, drops on food eaten this tick
//   fatigue    : grows with movement, decays when stationary
//   fear       : grows when predator within sense_radius, decays without
//   loneliness : decays with peer in sense_radius, grows alone
//
// Thirst is intentionally NOT updated here. The protagonist runtime
// derives thirst directly from SurvivalStatusLayer.hydrationRatio
// each reward step, so background species (no SurvivalStatusLayer)
// just keep thirst at 0.
//
// Passive: consumedOutputs() == 0. Cheap: O(N_agents) scan for fear /
// loneliness gating (same scan as AgentSocialPerception).

#include "core/interfaces/IAgentCapability.h"

#include <cstddef>

namespace neuro {

class DriveTickCapability final : public IAgentCapability {
public:
    struct Coefficients {
        // hunger: passive growth + per-unit-food drop.
        float hunger_growth_per_sec = 0.001f;
        float hunger_drop_per_food_unit = 0.01f;
        // fatigue: scales with |velocity|; rest decay constant.
        float fatigue_growth_per_speed_per_sec = 0.0008f;
        float fatigue_rest_decay_per_sec = 0.0006f;
        float fatigue_rest_speed_threshold = 0.05f;  // speed below which agent is "resting"
        // fear: predator-presence pulse + linear decay.
        float fear_predator_pulse = 0.5f;            // applied once per tick when predator in radius
        float fear_predator_growth_per_sec = 0.05f;  // continued growth per second of exposure
        float fear_decay_per_sec = 0.10f;            // decay per second when no predator
        // loneliness: peer-near drop + alone-grow.
        float loneliness_peer_drop_per_sec = 0.005f;
        float loneliness_alone_growth_per_sec = 0.001f;
    };

    explicit DriveTickCapability(float sense_radius = 24.0f,
                                 float metabolism = 1.0f,
                                 Coefficients coeffs = {});

    std::size_t consumedOutputs() const override { return 0; }
    void apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) override;
    const char* name() const override { return "drive_tick"; }

private:
    float sense_radius_;
    float metabolism_;
    Coefficients coeffs_;
};

}  // namespace neuro
