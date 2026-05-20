#pragma once

#include "core/interfaces/IAgentCapability.h"

#include <atomic>
#include <cstddef>

namespace neuro::routes::protagonist {

class ChopTreeCapability final : public IAgentCapability {
public:
    ChopTreeCapability(float chop_radius, float damage, float energy_cost, float activation_threshold = 0.5f);

    std::size_t consumedOutputs() const override { return 1; }
    void apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) override;
    const char* name() const override { return "chop_tree"; }

    static void resetCounters() {
        chop_attempts.store(0);
        chop_successes.store(0);
    }

    static inline std::atomic<std::size_t> chop_attempts{0};
    static inline std::atomic<std::size_t> chop_successes{0};

private:
    float chop_radius_;
    float damage_;
    float energy_cost_;
    float activation_threshold_;
};

}  // namespace neuro::routes::protagonist
