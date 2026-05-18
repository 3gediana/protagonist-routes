#include "core/evolution/selection/TournamentSelection.h"

#include <stdexcept>

namespace neuro {

TournamentSelection::TournamentSelection(std::size_t tournament_size, std::uint32_t seed)
    : tournament_size_(tournament_size == 0 ? 1 : tournament_size),
      rng_(seed == 0 ? std::random_device{}() : seed) {}

EvaluatedIndividual TournamentSelection::selectOne(const std::vector<EvaluatedIndividual>& evaluated) {
    if (evaluated.empty()) {
        throw std::invalid_argument("TournamentSelection requires a non-empty population");
    }

    std::uniform_int_distribution<std::size_t> index_dist(0, evaluated.size() - 1);
    EvaluatedIndividual best = evaluated[index_dist(rng_)];
    for (std::size_t i = 1; i < tournament_size_; ++i) {
        const auto candidate = evaluated[index_dist(rng_)];
        if (candidate.fitness > best.fitness
            || (candidate.fitness == best.fitness && candidate.complexity < best.complexity)) {
            best = candidate;
        }
    }
    return best;
}

}  // namespace neuro
