#pragma once

#include "core/interfaces/IEvolutionStrategy.h"

#include <cstddef>
#include <random>
#include <vector>

namespace neuro {

class TournamentSelection {
public:
    TournamentSelection(std::size_t tournament_size, std::uint32_t seed);

    EvaluatedIndividual selectOne(const std::vector<EvaluatedIndividual>& evaluated);

private:
    std::size_t tournament_size_;
    std::mt19937 rng_;
};

}  // namespace neuro
