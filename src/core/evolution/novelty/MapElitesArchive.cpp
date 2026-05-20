#include "core/evolution/novelty/MapElitesArchive.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace neuro {

MapElitesArchive::MapElitesArchive(std::size_t bins_per_dim,
                                   float value_min,
                                   float value_max)
    : bins_per_dim_(std::max<std::size_t>(2, bins_per_dim)),
      value_min_(value_min),
      value_max_(value_max <= value_min ? value_min + 1.0f : value_max) {}

std::uint64_t MapElitesArchive::cellKey(const BehaviorCharacterization& bc) const {
    const auto v = bc.toVector();
    const float range = value_max_ - value_min_;
    std::uint64_t key = 0;
    for (float x : v) {
        const float norm = std::clamp((x - value_min_) / range, 0.0f, 1.0f);
        // Lift to bin index in [0, bins_per_dim_-1].
        const std::size_t bin = std::min(bins_per_dim_ - 1,
                                          static_cast<std::size_t>(std::floor(norm * static_cast<float>(bins_per_dim_))));
        // Pack as a base-(bins_per_dim_) integer. For an 8-dim BC with
        // bins_per_dim_ up to ~16 this fits comfortably in 64 bits.
        key = key * static_cast<std::uint64_t>(bins_per_dim_) + static_cast<std::uint64_t>(bin);
    }
    return key;
}

bool MapElitesArchive::considerCandidate(GenomeId id,
                                         const BehaviorCharacterization& bc,
                                         float fitness) {
    const auto key = cellKey(bc);
    auto it = cells_.find(key);
    if (it == cells_.end()) {
        cells_.emplace(key, MapElitesElite{id, fitness, bc});
        return true;
    }
    if (fitness > it->second.fitness) {
        it->second = MapElitesElite{id, fitness, bc};
        return true;
    }
    return false;
}

void MapElitesArchive::clear() {
    cells_.clear();
}

float MapElitesArchive::bestFitness() const {
    if (cells_.empty()) return 0.0f;
    float best = -std::numeric_limits<float>::infinity();
    for (const auto& [_, elite] : cells_) {
        if (elite.fitness > best) best = elite.fitness;
    }
    return best;
}

float MapElitesArchive::avgFitness() const {
    if (cells_.empty()) return 0.0f;
    float sum = 0.0f;
    for (const auto& [_, elite] : cells_) sum += elite.fitness;
    return sum / static_cast<float>(cells_.size());
}

}  // namespace neuro
