#pragma once

#include "core/interfaces/IAgentCapability.h"

namespace neuro {

class AttackCapability final : public IAgentCapability {
public:
    AttackCapability(float attack_range,
                     float damage_factor,
                     float energy_transfer_rate,
                     float attack_cost,
                     float attack_threshold = 0.5f);

    std::size_t consumedOutputs() const override;
    void apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) override;
    const char* name() const override;

private:
    float attack_range_;
    float damage_factor_;
    float energy_transfer_rate_;
    float attack_cost_;
    float attack_threshold_;
};

}  // namespace neuro
