#include "core/reward/IntrinsicReward.h"

#include "core/agent/Agent.h"

namespace neuro {

IntrinsicReward::IntrinsicReward(double weight)
    : weight_(weight) {}

double IntrinsicReward::evaluate(const IAgent& agent, const IWorld&, float) const {
    if (const auto* concrete = dynamic_cast<const Agent*>(&agent)) {
        return concrete->discoveredNewCellThisTick() ? 1.0 : 0.0;
    }
    return 0.0;
}

double IntrinsicReward::weight() const {
    return weight_;
}

const char* IntrinsicReward::name() const {
    return "intrinsic";
}

}  // namespace neuro
