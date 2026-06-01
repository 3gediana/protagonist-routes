#pragma once

// L2.1 v2 (SPECIES_DIFFERENTIATION_SPEC.md § 1.4 / § 1.2 leopard row).
// Tracks how long the owning agent has been stationary (velocity
// magnitude < threshold) and flips its stealth_state once the
// stationary duration crosses ~3s. Movement (or a successful stealth
// strike, handled in PredatorHuntCapability) resets the timer and
// drops stealth.
//
// Passive: consumedOutputs() == 0. Attached only to ambush-class
// agents (background AmbushPredator). Cheap: O(1) per tick.

#include "core/interfaces/IAgentCapability.h"

namespace neuro::routes::protagonist {

class StealthCapability final : public IAgentCapability {
public:
    explicit StealthCapability(float stealth_threshold_seconds = 3.0f,
                               float velocity_epsilon = 0.05f)
        : stealth_threshold_seconds_(stealth_threshold_seconds),
          velocity_epsilon_(velocity_epsilon) {}

    std::size_t consumedOutputs() const override { return 0; }
    void apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) override;
    const char* name() const override { return "stealth_tracker"; }

private:
    float stealth_threshold_seconds_;
    float velocity_epsilon_;
};

}  // namespace neuro::routes::protagonist
