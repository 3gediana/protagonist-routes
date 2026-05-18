#include "core/reward/FoodReward.h"

#include "core/agent/Agent.h"

namespace neuro {

FoodReward::FoodReward(double weight)
    : weight_(weight) {}

double FoodReward::evaluate(const IAgent& agent, const IWorld&, float) const {
    if (const auto* concrete = dynamic_cast<const Agent*>(&agent)) {
        return static_cast<double>(concrete->foodEatenThisTick());
    }
    return 0.0;
}

double FoodReward::weight() const {
    return weight_;
}

const char* FoodReward::name() const {
    return "food";
}

}  // namespace neuro
