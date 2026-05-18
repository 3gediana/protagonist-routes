#pragma once

#include "core/interfaces/IEvolutionStrategy.h"

#include <cstddef>
#include <vector>

namespace neuro {

class EliteSelection {
public:
    explicit EliteSelection(double elite_ratio);

    std::vector<EvaluatedIndividual> select(const std::vector<EvaluatedIndividual>& evaluated) const;

private:
    double elite_ratio_;
};

}  // namespace neuro
