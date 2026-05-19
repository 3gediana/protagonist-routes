#include "core/reward/SurvivalReward.h"
#include "core/agent/Agent.h"

namespace neuro {

SurvivalReward::SurvivalReward(double weight)
    : weight_(weight) {}

double SurvivalReward::evaluate(const IAgent& agent, const IWorld&, float) const {
    const auto* concrete = dynamic_cast<const Agent*>(&agent);
    double reward = 0.01;
    // Agents with richer immune memory get survival bonus (recovered from more strains)
    if (concrete) {
        const auto strain_bonus = std::min(concrete->immuneMemory().size(), std::size_t(5));
        reward += 0.01 * static_cast<double>(strain_bonus);
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
