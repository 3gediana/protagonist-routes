#pragma once

#include "runtime/MigrationScheduler.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace neuro {
class IGenome;
}

namespace neuro::routes::protagonist {

// Layer 6 R2 (LAYER6_THREE_WORLDS_SPEC § 2.1): three-world topology
// from the FINAL_BLUEPRINT lifecycle vision. Every genome's life path
// is sandbox → borderland → main_world; the only way to enter the
// main world is by being promoted out of borderland (or, in extreme
// cases, by emergency rescue from main_world's own lineage).
enum class WorldRole : std::uint8_t {
    TrainingSandbox = 0,
    Borderland      = 1,
    MainWorld       = 2,
};

const char* worldRoleName(WorldRole role);

// One per-archetype NEAT pool plus the lineage IDs of those genomes,
// keyed in the same order. Owned by whichever WorldSlot it lives in.
struct WorldPopulationPool {
    std::vector<std::unique_ptr<IGenome>> genomes;
    std::vector<std::uint64_t> lineage_ids;
};

// All NEAT pools that belong to one of the three worlds. Map key is
// archetype tag (0 = protagonist, 1..N = background species).
struct WorldSlot {
    WorldRole role;
    std::vector<WorldPopulationPool> pools_by_archetype;
};

// Layer 6 R2 SKELETON (per spec § 6 priority 1):
//
// Real implementation will own three concrete World2D instances and
// a frame loop. This skeleton intentionally only owns the **NEAT pool
// state** for each world and the **migration interface**. The actual
// World2D wiring + tick loop is deferred to the next session because:
//   - sandbox already runs in ProtagonistEcologyRuntime
//   - borderland and main_world need full evaluator wiring + persistence
//     decisions that should not be hidden inside an incremental commit
//
// The skeleton is enough to:
//   1. unit-test the migration arithmetic (top-K promotion, lineage
//      preservation) without dragging in the rest of the runtime
//   2. give downstream code a stable type to extend toward the full
//      TripleWorldRuntime in the next session
class TripleWorldRuntime {
public:
    TripleWorldRuntime();

    // Promotes the top-K genomes (by fitness) of `archetype` from
    // sandbox to borderland. Returns the actual count promoted (may be
    // < k if the source pool is smaller).
    std::size_t promoteSandboxToBorderland(std::size_t archetype,
                                           const std::vector<double>& sandbox_fitnesses,
                                           std::size_t k);

    // Promotes the top-K genomes of `archetype` from borderland to
    // main_world. Same semantics as above. Should be called every
    // ~50 borderland generations (caller's responsibility).
    std::size_t promoteBorderlandToMain(std::size_t archetype,
                                        const std::vector<double>& borderland_fitnesses,
                                        std::size_t k);

    // Direct access to the per-world slots (for tests + the future
    // integration commit that will wire them into the real loop).
    WorldSlot& sandbox()    { return sandbox_; }
    WorldSlot& borderland() { return borderland_; }
    WorldSlot& mainWorld()  { return main_world_; }
    const WorldSlot& sandbox()    const { return sandbox_; }
    const WorldSlot& borderland() const { return borderland_; }
    const WorldSlot& mainWorld()  const { return main_world_; }

    // Helper for tests: install a sandbox pool for `archetype`.
    void seedSandboxPool(std::size_t archetype,
                         std::vector<std::unique_ptr<IGenome>> genomes,
                         std::vector<std::uint64_t> lineage_ids);

private:
    WorldPopulationPool& ensurePool(WorldSlot& slot, std::size_t archetype);

    WorldSlot sandbox_{WorldRole::TrainingSandbox, {}};
    WorldSlot borderland_{WorldRole::Borderland, {}};
    WorldSlot main_world_{WorldRole::MainWorld, {}};
};

}  // namespace neuro::routes::protagonist
