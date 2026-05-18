#pragma once

#include "core/interfaces/IRewardComponent.h"

namespace neuro {

class CombatReward final : public IRewardComponent {
public:
    CombatReward(double weight, float attack_reward, float kill_bonus, float death_penalty);

    double evaluate(const IAgent& agent, const IWorld& world, float dt) const override;
    double weight() const override;
    const char* name() const override;

private:
    double weight_;
    float attack_reward_;
    float kill_bonus_;
    float death_penalty_;
};

}  // namespace neuro
