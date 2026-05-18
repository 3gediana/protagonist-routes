#include "core/evolution/novelty/NoveltyArchive.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace neuro {

std::vector<float> BehaviorCharacterization::toVector() const {
    return {final_x, final_y, total_distance, avg_energy,
            visited_cells_ratio, food_eaten, damage_dealt, signals_emitted};
}

float BehaviorCharacterization::distance(const BehaviorCharacterization& a,
                                          const BehaviorCharacterization& b) {
    const auto va = a.toVector();
    const auto vb = b.toVector();
    float sum = 0.0f;
    for (std::size_t i = 0; i < va.size() && i < vb.size(); ++i) {
        const float d = va[i] - vb[i];
        sum += d * d;
    }
    return std::sqrt(sum);
}

NoveltyArchive::NoveltyArchive(std::size_t k_neighbors, float novelty_threshold)
    : k_neighbors_(k_neighbors), novelty_threshold_(novelty_threshold) {}

float NoveltyArchive::novelty(const BehaviorCharacterization& bc) const {
    if (archive_.empty()) return 1.0f;

    std::vector<float> distances;
    distances.reserve(archive_.size());
    for (const auto& entry : archive_) {
        distances.push_back(BehaviorCharacterization::distance(bc, entry));
    }

    std::partial_sort(distances.begin(),
                      distances.begin() + std::min(k_neighbors_, distances.size()),
                      distances.end());

    const std::size_t k = std::min(k_neighbors_, distances.size());
    float sum = 0.0f;
    for (std::size_t i = 0; i < k; ++i) {
        sum += distances[i];
    }
    return sum / static_cast<float>(k);
}

void NoveltyArchive::add(const BehaviorCharacterization& bc) {
    archive_.push_back(bc);
}

void NoveltyArchive::clear() {
    archive_.clear();
}

}  // namespace neuro
