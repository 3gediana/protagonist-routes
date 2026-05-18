#pragma once

#include "core/interfaces/IRewardComponent.h"

namespace neuro {

class SurvivalReward final : public IRewardComponent {
public:
    explicit SurvivalReward(double weight);

    double evaluate(const IAgent& agent, const IWorld& world, float dt) const override;
    double weight() const override;
    const char* name() const override;

private:
    double weight_;
};

}  // namespace neuro
