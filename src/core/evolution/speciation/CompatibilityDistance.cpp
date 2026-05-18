#include "core/evolution/speciation/CompatibilityDistance.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace neuro {

CompatibilityDistance::CompatibilityDistance(double excess_coefficient,
                                             double disjoint_coefficient,
                                             double weight_coefficient,
                                             std::size_t normalize_threshold)
    : excess_coefficient_(excess_coefficient),
      disjoint_coefficient_(disjoint_coefficient),
      weight_coefficient_(weight_coefficient),
      normalize_threshold_(normalize_threshold) {}

double CompatibilityDistance::compute(const NeatGenome& lhs, const NeatGenome& rhs) const {
    std::unordered_map<std::uint64_t, const ConnectionGene*> lhs_connections;
    lhs_connections.reserve(lhs.connections().size());
    std::uint64_t lhs_max = 0;
    for (const auto& connection : lhs.connections()) {
        lhs_connections.emplace(connection.innovation_id.value, &connection);
        lhs_max = std::max(lhs_max, connection.innovation_id.value);
    }

    std::unordered_map<std::uint64_t, const ConnectionGene*> rhs_connections;
    rhs_connections.reserve(rhs.connections().size());
    std::uint64_t rhs_max = 0;
    for (const auto& connection : rhs.connections()) {
        rhs_connections.emplace(connection.innovation_id.value, &connection);
        rhs_max = std::max(rhs_max, connection.innovation_id.value);
    }

    std::size_t excess = 0;
    std::size_t disjoint = 0;
    std::size_t matching = 0;
    double weight_delta_sum = 0.0;

    for (const auto& [innovation, lhs_connection] : lhs_connections) {
        const auto rhs_it = rhs_connections.find(innovation);
        if (rhs_it != rhs_connections.end()) {
            ++matching;
            weight_delta_sum += std::abs(static_cast<double>(lhs_connection->weight - rhs_it->second->weight));
            continue;
        }

        if (innovation > rhs_max) {
            ++excess;
        } else {
            ++disjoint;
        }
    }

    for (const auto& [innovation, rhs_connection] : rhs_connections) {
        (void)rhs_connection;
        if (lhs_connections.contains(innovation)) {
            continue;
        }

        if (innovation > lhs_max) {
            ++excess;
        } else {
            ++disjoint;
        }
    }

    const std::size_t larger = std::max(lhs.connectionCount(), rhs.connectionCount());
    const double normalizer = larger < normalize_threshold_ ? 1.0 : static_cast<double>(larger);
    const double average_weight_delta = matching == 0 ? 0.0 : weight_delta_sum / static_cast<double>(matching);

    return excess_coefficient_ * static_cast<double>(excess) / normalizer
         + disjoint_coefficient_ * static_cast<double>(disjoint) / normalizer
         + weight_coefficient_ * average_weight_delta;
}

}  // namespace neuro
