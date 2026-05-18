#include "core/reward/SurvivalReward.h"

namespace neuro {

SurvivalReward::SurvivalReward(double weight)
    : weight_(weight) {}

double SurvivalReward::evaluate(const IAgent&, const IWorld&, float) const {
    return 0.01;
}

double SurvivalReward::weight() const {
    return weight_;
}

const char* SurvivalReward::name() const {
    return "survival";
}

}  // namespace neuro
