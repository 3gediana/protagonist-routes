#include "runtime/MigrationScheduler.h"

#include <algorithm>
#include <stdexcept>

namespace neuro::routes::protagonist {

std::vector<MigrationCandidate> MigrationScheduler::extractTopK(
    const std::vector<std::unique_ptr<IGenome>>& source_genomes,
    const std::vector<double>& source_fitnesses,
    const std::vector<std::uint64_t>& source_lineages,
    std::size_t k) {
    if (source_genomes.size() != source_fitnesses.size()
        || source_genomes.size() != source_lineages.size()) {
        throw std::runtime_error("MigrationScheduler::extractTopK: input vectors must be the same size");
    }
    const std::size_t n = source_genomes.size();
    if (n == 0 || k == 0) return {};

    // Stable order so ties are deterministic (lower index wins).
    std::vector<std::size_t> indices(n);
    for (std::size_t i = 0; i < n; ++i) indices[i] = i;
    std::stable_sort(indices.begin(), indices.end(),
                     [&](std::size_t a, std::size_t b) {
                         return source_fitnesses[a] > source_fitnesses[b];
                     });

    const std::size_t take = std::min(k, n);
    std::vector<MigrationCandidate> out;
    out.reserve(take);
    for (std::size_t i = 0; i < take; ++i) {
        const std::size_t idx = indices[i];
        if (source_genomes[idx] == nullptr) continue;
        out.push_back(MigrationCandidate{
            source_genomes[idx]->clone(),
            source_fitnesses[idx],
            source_lineages[idx]});
    }
    return out;
}

void MigrationScheduler::seedFromCandidates(
    std::vector<MigrationCandidate> candidates,
    std::vector<std::unique_ptr<IGenome>>& target_pop,
    std::vector<std::uint64_t>& target_lineages,
    std::vector<double>* target_fitnesses) {
    target_pop.reserve(target_pop.size() + candidates.size());
    target_lineages.reserve(target_lineages.size() + candidates.size());
    if (target_fitnesses != nullptr) {
        target_fitnesses->reserve(target_fitnesses->size() + candidates.size());
    }
    for (auto& c : candidates) {
        if (c.genome == nullptr) continue;
        target_lineages.push_back(c.lineage_id);
        if (target_fitnesses != nullptr) {
            target_fitnesses->push_back(c.fitness);
        }
        target_pop.push_back(std::move(c.genome));
    }
}

void MigrationScheduler::promoteTopK(
    const std::vector<std::unique_ptr<IGenome>>& source_genomes,
    const std::vector<double>& source_fitnesses,
    const std::vector<std::uint64_t>& source_lineages,
    std::size_t k,
    std::vector<std::unique_ptr<IGenome>>& target_pop,
    std::vector<std::uint64_t>& target_lineages,
    std::vector<double>* target_fitnesses) {
    auto candidates = extractTopK(source_genomes, source_fitnesses, source_lineages, k);
    seedFromCandidates(std::move(candidates), target_pop, target_lineages, target_fitnesses);
}

}  // namespace neuro::routes::protagonist
