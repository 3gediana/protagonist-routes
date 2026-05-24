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

}  // namespace neuro::routes::protagonist
