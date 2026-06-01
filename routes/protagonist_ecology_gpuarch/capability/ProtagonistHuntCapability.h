#pragma once

#include "core/interfaces/IAgentCapability.h"

#include <atomic>
#include <cstddef>

namespace neuro::routes::protagonist {

class ProtagonistHuntCapability final : public IAgentCapability {
public:
    ProtagonistHuntCapability(float hunt_radius, float damage, float energy_gain, float cost, bool enabled = true);

    std::size_t consumedOutputs() const override { return 1; }
    void apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) override;
    const char* name() const override { return "protagonist_hunt"; }

    static void resetCounters() {
        hunt_attempts.store(0);
        hunt_successes.store(0);
    }

    static inline std::atomic<std::size_t> hunt_attempts{0};
    static inline std::atomic<std::size_t> hunt_successes{0};

private:
    float hunt_radius_;
    float damage_;
    float energy_gain_;
    float cost_;
    bool enabled_;
};

}  // namespace neuro::routes::protagonist
