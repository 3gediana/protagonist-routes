#pragma once

#include "runtime/MigrationScheduler.h"

#include <cstddef>
#include <cstdint>
#include <functional>
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
//
// L6 R2 stage 2: also carry the fitness that earned each genome its
// promotion. The borderland slot needs this so that the next promote
// (borderland→main_world) can re-rank without re-evaluation.
//
// L6 R3 stage 3 commit C: borderland_eval_count tracks how many times
// evaluateBorderland has been called for this archetype's pool. The
// 50-generation 物种延续 gate (spec § 1.2) uses this to decide whether
// the archetype has spent enough time in borderland to be promoted to
// main_world. Only borderland slots care about this field; sandbox and
// main_world leave it at 0.
struct WorldPopulationPool {
    std::vector<std::unique_ptr<IGenome>> genomes;
    std::vector<std::uint64_t> lineage_ids;
    std::vector<double> fitnesses;
    std::size_t borderland_eval_count{0};
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

    // L6 R2 stage 2: integration entry point.
    //
    // ProtagonistEcologyRuntime owns the actual sandbox pool (its
    // `population` and `species_contexts[i].population`), so we don't
    // mirror it inside this runtime. Instead, at the end of each
    // generation, the caller hands us the live pool + fitnesses; we
    // pick the top-K, clone them into the borderland slot, and stash
    // their fitnesses there so a later borderland→main_world promote
    // can re-rank without re-evaluating.
    //
    // `lineage_ids` should be the same length as `source_genomes`; if
    // the caller doesn't track lineage separately, passing genome ids
    // is a sensible placeholder (every genome is its own lineage tip).
    std::size_t promoteExternalToBorderland(
        std::size_t archetype,
        const std::vector<std::unique_ptr<IGenome>>& source_genomes,
        const std::vector<double>& fitnesses,
        const std::vector<std::uint64_t>& lineage_ids,
        std::size_t k);

    // L6 R2 stage 2: stored-fitness overload. Reads the fitnesses that
    // were stashed during promoteExternalToBorderland (or
    // promoteSandboxToBorderland) and uses them to re-rank borderland
    // genomes for promotion to main_world. Useful when borderland is
    // an accumulator without its own evaluation loop.
    std::size_t promoteBorderlandToMain(std::size_t archetype, std::size_t k);

    // L6 R2 stage 3 commit 1: cap the borderland slot's pool size.
    //
    // Under the stage-2 protocol, every generation top_k_per_gen new
    // genomes are appended to borderland and never removed, so after
    // 60 generations a borderland pool can hold 240+ protagonist
    // genomes per archetype. Cull keeps borderland bounded by retaining
    // only the top `max_pool_size` by stashed fitness (the same
    // fitness that promoteBorderlandToMain ranks on, so promotion
    // semantics are preserved across the cull).
    //
    // Returns the number of genomes that were dropped. If the pool
    // already fits or is malformed, returns 0 and changes nothing.
    //
    // NOTE: this is *not* "borderland evolution". Spec § 2.2 calls for
    // an actual world tick + 50-generation 物种延续 test inside
    // borderland; that requires running an independent World2D per
    // archetype and is deferred to Layer 6 R3. This commit only
    // implements the pruning half of "borderland is bounded".
    std::size_t cullBorderland(std::size_t archetype, std::size_t max_pool_size);

    // L6 R3 stage 3 commit A: in-place borderland evaluation.
    //
    // Spec § 1.2 says borderland is "50 代考验 + 不灭绝才进入主世界".
    // Stage 2 + R2-stage-3 left borderland as a fitness pool accumulator
    // — every genome's stashed fitness was the snapshot taken at the
    // moment of sandbox→borderland promotion, never re-evaluated. This
    // call walks the borderland pool of `archetype`, invokes `eval_fn`
    // for each genome (caller provides the World2D / brain machinery),
    // and updates the stashed fitness so subsequent
    // promoteBorderlandToMain / cullBorderland reflect the borderland's
    // own selection pressure rather than the sandbox snapshot.
    //
    // The old stashed fitness is first multiplied by `aging_factor`
    // (default 1.0 = no aging) so that a genome which stops performing
    // gradually loses its lead instead of being permanently insulated
    // by an old high score. The recorded fitness is `max(aged_old,
    // fresh)` so a single bad evaluation doesn't immediately demote a
    // proven survivor — borderland is a survival test, not a single
    // make-or-break exam.
    //
    // Returns the number of genomes evaluated (0 if pool empty or
    // eval_fn is null). The caller is expected to follow up with
    // cullBorderland(archetype, max_pool_size) to actually evict the
    // low-fitness tail.
    using BorderlandEvalFn = std::function<double(const IGenome&)>;
    std::size_t evaluateBorderland(std::size_t archetype,
                                   const BorderlandEvalFn& eval_fn,
                                   double aging_factor = 1.0);

    // L6 R3 stage 3 commit J: borderland in-place mutation. Walk every
    // genome in the archetype's borderland pool and apply `mutator` to
    // it. Stashed fitness is NOT touched; the next evaluateBorderland
    // call will overwrite it (with max(aged, fresh) semantics). The
    // mutation should be small enough that the existing stashed fitness
    // remains a reasonable upper bound while fresh evaluations have a
    // chance to discover whether the perturbation helped.
    //
    // Returns the number of genomes mutated (0 if pool empty or
    // mutator is null).
    using BorderlandMutateFn = std::function<void(IGenome&)>;
    std::size_t mutateBorderlandPool(std::size_t archetype,
                                     const BorderlandMutateFn& mutator);

    // L6 R2 stage 3 commit 3: emergency rescue when a main_world
    // archetype pool is empty (extinction = spec invariant #4).
    // Pulls the top-K genomes (by stashed fitness) from the borderland
    // slot of the same archetype into main_world. Borderland retains
    // its copies (rescue is a clone, not a move) so subsequent normal
    // promote steps keep working. Returns the number of rescued
    // genomes (0 if main_world wasn't actually empty, or borderland
    // had nothing to give).
    std::size_t rescueExtinctSpecies(std::size_t archetype, std::size_t k);

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
