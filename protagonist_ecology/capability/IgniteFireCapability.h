#pragma once

// Agent-driven fire making (realism, 2026-06): a protagonist lights / refuels
// a nearby campfire on demand by spending firewood (a Stick), rather than the
// world handing out lightning-lit fires for free. This is the "ignite" skill:
//   - claims one NEAT output (consumedOutputs() == 1); fires when output >= thr
//   - requires a Stick source (a loose stick within reach, or the stockpile)
//   - requires a campfire site within ignite_radius; on success the site is lit
//     and gains fuel_per_stick seconds of burn (capped in CampfireLayer)
//   - costs energy_cost energy
//
// Pairs with CampfireLayer's fuel model: lit fires burn their fuel down and go
// dormant, so the agent has to keep feeding sticks to keep warmth alive.

#include "core/interfaces/IAgentCapability.h"

#include <atomic>
#include <cstddef>

namespace neuro::routes::protagonist {

class IgniteFireCapability final : public IAgentCapability {
public:
    IgniteFireCapability(float ignite_radius = 6.0f,
                         float fuel_per_stick = 150.0f,
                         float energy_cost = 4.0f,
                         float activation_threshold = 0.5f);

    std::size_t consumedOutputs() const override { return 1; }
    void apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) override;
    const char* name() const override { return "ignite_fire"; }

    static void resetCounters() {
        ignite_attempts.store(0);
        ignite_successes.store(0);
    }

    static inline std::atomic<std::size_t> ignite_attempts{0};
    static inline std::atomic<std::size_t> ignite_successes{0};

private:
    float ignite_radius_;
    float fuel_per_stick_;
    float energy_cost_;
    float activation_threshold_;
};

}  // namespace neuro::routes::protagonist
