#pragma once

#include "core/interfaces/IGenome.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace neuro::routes::protagonist {

// Layer 6 R2 (LAYER6_THREE_WORLDS_SPEC § 3): one elite "package" being
// promoted from a lower world to a higher world. The genome is owned
// (cloned out of the source pool); fitness is the score that earned
// the promotion; lineage_id is preserved so the receiving world can
// merge bloodlines correctly.
struct MigrationCandidate {
    std::unique_ptr<IGenome> genome;
    double fitness{0.0};
    std::uint64_t lineage_id{0};
};

// Layer 6 R2 skeleton (per spec § 6 priority 1): "prove the three-world
// flow works, defer disk persistence". Only the in-memory promotion
// path is implemented here. Real-world wiring to ProtagonistEcologyRuntime
// is deferred to a follow-up session per the spec's Stage-1 plan.
//
// The scheduler is **stateless across calls** — promote/seed each take
// the relevant pools as arguments rather than owning them, which keeps
// the implementation testable in isolation.
class MigrationScheduler {
public:
    // Pick the top-K (by fitness) genomes from `source_genomes`/
    // `source_fitnesses`/`source_lineages`, clone them, and return
    // the resulting MigrationCandidates. Sizes must match. K is
    // clamped to source_genomes.size().
    static std::vector<MigrationCandidate> extractTopK(
        const std::vector<std::unique_ptr<IGenome>>& source_genomes,
        const std::vector<double>& source_fitnesses,
        const std::vector<std::uint64_t>& source_lineages,
        std::size_t k);

    // Move the candidate genomes into `target_pop` (clone is already
    // done by extractTopK). Lineage IDs are stored in `target_lineages`
    // in the same order. All target vectors are appended to (not
    // cleared). `target_fitnesses` is optional; when non-null, the
    // candidate fitness is recorded so a downstream pass can re-rank
    // without re-evaluation (L6 R2 stage 2 borderland→main_world flow).
    static void seedFromCandidates(
        std::vector<MigrationCandidate> candidates,
        std::vector<std::unique_ptr<IGenome>>& target_pop,
        std::vector<std::uint64_t>& target_lineages,
        std::vector<double>* target_fitnesses = nullptr);

    // Convenience: chain extractTopK + seedFromCandidates in one go.
    static void promoteTopK(
        const std::vector<std::unique_ptr<IGenome>>& source_genomes,
        const std::vector<double>& source_fitnesses,
        const std::vector<std::uint64_t>& source_lineages,
        std::size_t k,
        std::vector<std::unique_ptr<IGenome>>& target_pop,
        std::vector<std::uint64_t>& target_lineages,
        std::vector<double>* target_fitnesses = nullptr);
};

}  // namespace neuro::routes::protagonist
