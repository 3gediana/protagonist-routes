#include "core/reward/IntrinsicReward.h"

#include "core/agent/Agent.h"

#include <algorithm>
#include <cmath>

namespace neuro {

IntrinsicReward::IntrinsicReward(double weight)
    : weight_(weight) {}

double IntrinsicReward::evaluate(const IAgent& agent, const IWorld&, float) const {
    if (const auto* concrete = dynamic_cast<const Agent*>(&agent)) {
        if (!concrete->discoveredNewCellThisTick()) {
            return 0.0;
        }

        const double visited_cells = static_cast<double>(std::max<std::size_t>(std::size_t{1}, concrete->visitedCellCount()));
        return 1.0 / std::sqrt(visited_cells);
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
