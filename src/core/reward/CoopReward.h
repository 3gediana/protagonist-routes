#pragma once

#include "core/interfaces/IRewardComponent.h"

namespace neuro {

class CoopReward final : public IRewardComponent {
public:
    CoopReward(double weight, double proximity_bonus);

    double evaluate(const IAgent& agent, const IWorld& world, float dt) const override;
    double weight() const override;
    const char* name() const override;

private:
    double weight_;
    double proximity_bonus_;
};

}  // namespace neuro
