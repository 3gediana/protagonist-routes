#include "core/reward/SurvivalReward.h"
#include "core/agent/Agent.h"

namespace neuro {

SurvivalReward::SurvivalReward(double weight)
    : weight_(weight) {}

double SurvivalReward::evaluate(const IAgent& agent, const IWorld&, float) const {
    const auto* concrete = dynamic_cast<const Agent*>(&agent);
    double reward = 0.01;
    // Immune agents get a survival bonus (recovered from disease)
    if (concrete && concrete->immune()) {
        reward += 0.02;
    }
    return reward;
}

double SurvivalReward::weight() const {
    return weight_;
}

const char* SurvivalReward::name() const {
    return "survival";
}

}  // namespace neuro
