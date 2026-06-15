#pragma once

#include "core/interfaces/IAgentCapability.h"

#include <atomic>
#include <cstddef>

namespace neuro::routes::protagonist {

class CraftSpearCapability final : public IAgentCapability {
public:
    CraftSpearCapability(float energy_cost, float activation_threshold = 0.5f,
                         bool require_in_base = true, bool require_stone = true,
                         bool respect_worksite_reservation = true);

    std::size_t consumedOutputs() const override { return 1; }
    void apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) override;
    const char* name() const override { return "craft_spear"; }

    static void resetCounters() {
        craft_attempts.store(0);
        craft_successes.store(0);
    }

    static inline std::atomic<std::size_t> craft_attempts{0};
    static inline std::atomic<std::size_t> craft_successes{0};

private:
    float energy_cost_;
    float activation_threshold_;
    bool require_in_base_;
    bool require_stone_;
    bool respect_worksite_reservation_;
};

}  // namespace neuro::routes::protagonist
