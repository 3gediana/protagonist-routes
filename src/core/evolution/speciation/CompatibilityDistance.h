#pragma once

#include "core/brain/neat/NeatGenome.h"

namespace neuro {

class CompatibilityDistance {
public:
    CompatibilityDistance(double excess_coefficient,
                          double disjoint_coefficient,
                          double weight_coefficient,
                          std::size_t normalize_threshold = 20);

    double compute(const NeatGenome& lhs, const NeatGenome& rhs) const;

private:
    double excess_coefficient_;
    double disjoint_coefficient_;
    double weight_coefficient_;
    std::size_t normalize_threshold_;
};

}  // namespace neuro
