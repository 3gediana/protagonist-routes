#pragma once

#include "core/interfaces/IRewardComponent.h"

namespace neuro {

// Rewards agents for being in terrain matching their habitat preference gene.
// habitat_pref: 0=lowland, 1=highland
// Reward = weight * match_score * dt
// match_score = 1 - |normalized_height - habitat_pref|
class HabitatReward final : public IRewardComponent {
public:
    HabitatReward(double weight, float height_scale);

    double evaluate(const IAgent& agent, const IWorld& world, float dt) const override;
    double weight() const override { return weight_; }
    const char* name() const override { return "habitat"; }

private:
    double weight_;
    float height_scale_;
};

// Rewards agents for being active during their preferred time of day.
// activity_period: 0=nocturnal, 1=diurnal
// Reward = weight * time_match * dt
class ActivityReward final : public IRewardComponent {
public:
    ActivityReward(double weight);

    double evaluate(const IAgent& agent, const IWorld& world, float dt) const override;
    double weight() const override { return weight_; }
    const char* name() const override { return "activity"; }

private:
    double weight_;
};

}  // namespace neuro
