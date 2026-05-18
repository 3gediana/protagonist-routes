#include "core/evolution/selection/EliteSelection.h"

#include <algorithm>

namespace neuro {

EliteSelection::EliteSelection(double elite_ratio)
    : elite_ratio_(elite_ratio) {}

std::vector<EvaluatedIndividual> EliteSelection::select(const std::vector<EvaluatedIndividual>& evaluated) const {
    auto sorted = evaluated;
    std::sort(sorted.begin(), sorted.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.fitness != rhs.fitness) {
            return lhs.fitness > rhs.fitness;
        }
        return lhs.complexity < rhs.complexity;
    });

    const std::size_t count = std::max<std::size_t>(1, static_cast<std::size_t>(sorted.size() * elite_ratio_));
    if (count < sorted.size()) {
        sorted.resize(count);
    }
    return sorted;
}

}  // namespace neuro
