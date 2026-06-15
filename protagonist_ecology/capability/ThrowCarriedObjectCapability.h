#pragma once

#include "core/interfaces/IAgentCapability.h"

#include <atomic>
#include <cstddef>

namespace neuro::routes::protagonist {

class ThrowCarriedObjectCapability final : public IAgentCapability {
public:
    ThrowCarriedObjectCapability(float throw_speed, float throw_radius, float stone_damage, float spear_damage, float energy_cost, float stick_damage = 4.0f);

    std::size_t consumedOutputs() const override { return 2; }
    void apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) override;
    const char* name() const override { return "throw_carried_object"; }

    static void resetCounters() {
        throw_attempts.store(0);
        throw_hits.store(0);
    }

    static inline std::atomic<std::size_t> throw_attempts{0};
    static inline std::atomic<std::size_t> throw_hits{0};

private:
    float throw_speed_;
    float throw_radius_;
    float stone_damage_;
    float spear_damage_;
    float energy_cost_;
    float stick_damage_;
};

}  // namespace neuro::routes::protagonist
