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

std::size_t TripleWorldRuntime::evaluateBorderland(
    std::size_t archetype,
    const BorderlandEvalFn& eval_fn,
    double aging_factor) {
    auto& pool = ensurePool(borderland_, archetype);
    if (pool.genomes.empty() || !eval_fn) return 0;

    // Defensive: stashed fitness array must align with genomes. If
    // somehow desynced (e.g. caller bypassed promoteExternalToBorderland)
    // fall back to zero so the new fresh evaluation drives the result.
    if (pool.fitnesses.size() != pool.genomes.size()) {
        pool.fitnesses.assign(pool.genomes.size(), 0.0);
    }

    std::size_t evaluated = 0;
    for (std::size_t i = 0; i < pool.genomes.size(); ++i) {
        if (!pool.genomes[i]) continue;  // defensive
        const double aged  = pool.fitnesses[i] * aging_factor;
        const double fresh = eval_fn(*pool.genomes[i]);
        pool.fitnesses[i] = std::max(aged, fresh);
        ++evaluated;
    }
    // L6 R3 stage 3 commit C: bump the per-archetype eval counter even
    // if the pool was non-empty but every entry was nulled out (rare
    // defensive path). The counter is used by the 50-gen 物种延续 gate
    // in promoteBorderlandToMain.
    ++pool.borderland_eval_count;
    return evaluated;
}

std::size_t TripleWorldRuntime::mutateBorderlandPool(
    std::size_t archetype, const BorderlandMutateFn& mutator) {
    auto& pool = ensurePool(borderland_, archetype);
    if (pool.genomes.empty() || !mutator) return 0;
    std::size_t mutated = 0;
    for (auto& g : pool.genomes) {
        if (!g) continue;
        mutator(*g);
        ++mutated;
    }
    return mutated;
}

std::size_t TripleWorldRuntime::rescueExtinctSpecies(std::size_t archetype, std::size_t k) {
    auto& main_pool = ensurePool(main_world_, archetype);
    // Only fire when main_world is genuinely extinct for this archetype.
    if (!main_pool.genomes.empty()) return 0;

    auto& border_pool = ensurePool(borderland_, archetype);
    if (border_pool.genomes.empty() || k == 0) return 0;

    // Use stashed fitness if aligned; otherwise treat as FIFO (equal-fitness
    // stable sort = source order preserved = oldest borderland entries
    // rescue first).
    std::vector<double> fallback_fits;
    const std::vector<double>* fits_ptr = &border_pool.fitnesses;
    if (border_pool.fitnesses.size() != border_pool.genomes.size()) {
        fallback_fits.assign(border_pool.genomes.size(), 1.0);
        fits_ptr = &fallback_fits;
    }

    auto candidates = MigrationScheduler::extractTopK(
        border_pool.genomes, *fits_ptr, border_pool.lineage_ids, k);
    const std::size_t n = candidates.size();
    MigrationScheduler::seedFromCandidates(
        std::move(candidates), main_pool.genomes, main_pool.lineage_ids,
        &main_pool.fitnesses);
    return n;
}

}  // namespace neuro::routes::protagonist
