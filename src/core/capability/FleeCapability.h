#pragma once

#include "core/interfaces/IAgentCapability.h"
#include "core/types/Aliases.h"

namespace neuro {

class FleeCapability final : public IAgentCapability {
public:
    // Two-stage prey flee mode ("被打才逃+喘息窗口"). Default-constructed
    // params reproduce the exact legacy flee behavior (enabled=false), so
    // existing callers are unchanged unless they opt in explicitly.
    struct TwoStageFleeParams {
        bool enabled = false;
        float burst_seconds = 0.4f;          // flee at full boost for this long
        float rest_seconds = 2.5f;           // then rest (no flee movement) this long
        float damage_memory_seconds = 6.0f;  // protagonist-like hunters are threats only within this window after last damage
    };

    FleeCapability(float flee_radius, float flee_speed_boost, float flee_cost,
                   TwoStageFleeParams params = {});

    std::size_t consumedOutputs() const override;
    void apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) override;
    const char* name() const override;

private:
    float flee_radius_;
    float flee_speed_boost_;
    float flee_cost_;
    TwoStageFleeParams params_;
    // Burst/rest cycle state (two-stage mode only).
    bool cycle_active_ = false;
    bool resting_ = false;
    SimTimeSeconds phase_started_ = 0.0;
};

}  // namespace neuro
