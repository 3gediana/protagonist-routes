#include "runtime/TripleWorldRuntime.h"

#include "core/interfaces/IGenome.h"

#include <algorithm>

namespace neuro::routes::protagonist {

const char* worldRoleName(WorldRole role) {
    switch (role) {
        case WorldRole::TrainingSandbox: return "training_sandbox";
        case WorldRole::Borderland:      return "borderland";
        case WorldRole::MainWorld:       return "main_world";
    }
    return "unknown";
}

TripleWorldRuntime::TripleWorldRuntime() = default;

WorldPopulationPool& TripleWorldRuntime::ensurePool(WorldSlot& slot, std::size_t archetype) {
    if (slot.pools_by_archetype.size() <= archetype) {
        slot.pools_by_archetype.resize(archetype + 1);
    }
    return slot.pools_by_archetype[archetype];
}

void TripleWorldRuntime::seedSandboxPool(std::size_t archetype,
                                         std::vector<std::unique_ptr<IGenome>> genomes,
                                         std::vector<std::uint64_t> lineage_ids) {
    auto& pool = ensurePool(sandbox_, archetype);
    pool.genomes = std::move(genomes);
    pool.lineage_ids = std::move(lineage_ids);
}

std::size_t TripleWorldRuntime::promoteSandboxToBorderland(
    std::size_t archetype,
    const std::vector<double>& sandbox_fitnesses,
    std::size_t k) {
    auto& src = ensurePool(sandbox_, archetype);
    auto& dst = ensurePool(borderland_, archetype);
    if (src.genomes.empty() || k == 0) return 0;

    auto candidates = MigrationScheduler::extractTopK(
        src.genomes, sandbox_fitnesses, src.lineage_ids, k);
    const std::size_t n = candidates.size();
    MigrationScheduler::seedFromCandidates(
        std::move(candidates), dst.genomes, dst.lineage_ids, &dst.fitnesses);
    return n;
}

std::size_t TripleWorldRuntime::promoteBorderlandToMain(
    std::size_t archetype,
    const std::vector<double>& borderland_fitnesses,
    std::size_t k) {
    auto& src = ensurePool(borderland_, archetype);
    auto& dst = ensurePool(main_world_, archetype);
    if (src.genomes.empty() || k == 0) return 0;

    auto candidates = MigrationScheduler::extractTopK(
        src.genomes, borderland_fitnesses, src.lineage_ids, k);
    const std::size_t n = candidates.size();
    MigrationScheduler::seedFromCandidates(
        std::move(candidates), dst.genomes, dst.lineage_ids, &dst.fitnesses);
    return n;
}

std::size_t TripleWorldRuntime::promoteExternalToBorderland(
    std::size_t archetype,
    const std::vector<std::unique_ptr<IGenome>>& source_genomes,
    const std::vector<double>& fitnesses,
    const std::vector<std::uint64_t>& lineage_ids,
    std::size_t k) {
    auto& dst = ensurePool(borderland_, archetype);
    if (source_genomes.empty() || k == 0) return 0;

    auto candidates = MigrationScheduler::extractTopK(
        source_genomes, fitnesses, lineage_ids, k);
    const std::size_t n = candidates.size();
    MigrationScheduler::seedFromCandidates(
        std::move(candidates), dst.genomes, dst.lineage_ids, &dst.fitnesses);
    return n;
}

std::size_t TripleWorldRuntime::promoteBorderlandToMain(std::size_t archetype, std::size_t k) {
    auto& src = ensurePool(borderland_, archetype);
    if (src.genomes.empty() || k == 0) return 0;
    if (src.fitnesses.size() != src.genomes.size()) {
        // Fitness wasn't tracked for this slot (shouldn't happen via the
        // documented APIs, but be defensive). Treat all as equal-fitness
        // FIFO so the oldest entries promote first.
        return promoteBorderlandToMain(archetype,
                                       std::vector<double>(src.genomes.size(), 1.0),
                                       k);
    }
    return promoteBorderlandToMain(archetype, src.fitnesses, k);
}

std::size_t TripleWorldRuntime::cullBorderland(std::size_t archetype, std::size_t max_pool_size) {
    auto& pool = ensurePool(borderland_, archetype);
    if (pool.genomes.size() <= max_pool_size) return 0;
    // Defensive: aligned tracking arrays are required to cull by fitness.
    if (pool.fitnesses.size() != pool.genomes.size()
        || pool.lineage_ids.size() != pool.genomes.size()) {
        return 0;
    }

    std::vector<std::size_t> indices(pool.genomes.size());
    for (std::size_t i = 0; i < indices.size(); ++i) indices[i] = i;
    std::stable_sort(indices.begin(), indices.end(),
                     [&](std::size_t a, std::size_t b) {
                         return pool.fitnesses[a] > pool.fitnesses[b];
                     });

    std::vector<std::unique_ptr<IGenome>> kept_genomes;
    std::vector<std::uint64_t> kept_lineages;
    std::vector<double> kept_fitnesses;
    kept_genomes.reserve(max_pool_size);
    kept_lineages.reserve(max_pool_size);
    kept_fitnesses.reserve(max_pool_size);
    for (std::size_t i = 0; i < max_pool_size; ++i) {
        const std::size_t idx = indices[i];
        kept_genomes.push_back(std::move(pool.genomes[idx]));
        kept_lineages.push_back(pool.lineage_ids[idx]);
        kept_fitnesses.push_back(pool.fitnesses[idx]);
    }

    const std::size_t dropped = pool.genomes.size() - max_pool_size;
    pool.genomes = std::move(kept_genomes);
    pool.lineage_ids = std::move(kept_lineages);
    pool.fitnesses = std::move(kept_fitnesses);
    return dropped;
}

}  // namespace neuro::routes::protagonist
