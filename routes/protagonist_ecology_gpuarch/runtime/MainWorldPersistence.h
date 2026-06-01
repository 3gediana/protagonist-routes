#pragma once

#include "runtime/TripleWorldRuntime.h"
#include "world/NurseryProgressionLayer.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace neuro::routes::protagonist {

// Layer 6 R2 stage 3 commit 2: pool-only snapshot persistence for
// the TripleWorldRuntime borderland + main_world slots.
//
// Scope of THIS commit (deliberately narrow):
//   - serialize borderland.pools_by_archetype + main_world.pools_by_archetype
//     as a manifest JSON + one binary blob per genome (using the existing
//     NeatGenome / ProtagonistGenome saveToFile format)
//   - reload the same layout back into a TripleWorldRuntime, preserving
//     genome IDs, lineage IDs, fitness, and archetype-pool placement
//
// NOT in scope (per LAYER6_THREE_WORLDS_SPEC § 2.4 "full MainWorldSnapshot"):
//   - agent positions / drives / DNC bank state
//   - worksites / trees / waters / weather / season
//   - building progress / stockpile
// Those fields require a live World2D in main_world (deferred to R3) to
// have any meaning. With borderland/main_world currently being pool
// accumulators, all we *can* truthfully persist is the pool itself.
//
// On-disk layout under `persist_dir`:
//
//   persist_dir/
//     main_world_manifest.json
//     borderland/archetype_K/genome_<i>.bin
//     main_world/archetype_K/genome_<i>.bin
//
// where K is the archetype index and <i> is the position in the pool.
// Genome blobs reuse the existing per-genome saveToFile binary format
// (NeatGenome for archetype >= 1, ProtagonistGenome for archetype 0).

struct MainWorldSnapshotMetadata {
    std::uint64_t snapshot_id{0};
    double sim_time_seconds{0.0};
    std::size_t generations_elapsed{0};
    std::string scenario_name;
    // Stone Bootstrap commit 3 (STONE_BOOTSTRAP_PERSISTENCE_PROPOSAL.md
    // option D §4.5): per-archetype curriculum unlock state. Empty
    // vector means "no progression state persisted" (legacy snapshots
    // load fine; the runtime then keeps its default-false state).
    // Saved one entry per archetype index (0 = protagonist).
    std::vector<ArchetypeProgressionState> archetype_progression;
};

class MainWorldPersistence {
public:
    // Write manifest + per-genome blobs under `persist_dir`. Creates
    // directories as needed. Returns true on success.
    static bool save(const TripleWorldRuntime& triple_world,
                     const MainWorldSnapshotMetadata& meta,
                     const std::filesystem::path& persist_dir);

    // Read manifest + per-genome blobs under `persist_dir` and populate
    // `triple_world` (replacing whatever borderland/main_world contents
    // were there). Sandbox is not touched. Returns true if the manifest
    // was parseable and all referenced blobs loaded; partial loads
    // return false and `triple_world` is left in its prior state.
    static bool load(const std::filesystem::path& persist_dir,
                     TripleWorldRuntime& triple_world,
                     MainWorldSnapshotMetadata* out_meta = nullptr);
};

}  // namespace neuro::routes::protagonist
