#pragma once

#include "runtime/BootstrapPopulation.h"
#include "runtime/ProtagonistEcologyConfig.h"
#include "runtime/TripleWorldRuntime.h"

#include <cstddef>
#include <vector>

namespace neuro::routes::protagonist {

// Layer 6 R3 stage 3 commit D (skeleton): centralizes the borderland
// in-place evaluation logic so the main training loop in
// ProtagonistEcologyRuntime no longer carries an inline no-op identity
// callback. This is the entry point that future commits will turn into
// a real World2D mini-tick evaluator.
//
// Current implementation is **still a placeholder**: it scores each
// genome by `complexity() * 100` so the stashed fitness moves at all
// (instead of being literally identity), but does NOT yet build a
// borderland World2D and tick. That is the next commit's job and
// requires significant new wiring (createBootstrapWorld + agent
// injection + brain forward + episode-end fitness extraction).
//
// Why a placeholder this commit? See LAYER6_THREE_WORLDS_SPEC § 6
// priority list — spec says "do migration arithmetic before world
// tick to keep commits incremental and not destabilize the training
// loop". This commit makes the call site `BorderlandEvaluator::
// evaluatePool(...)` so commit E only has to swap the implementation
// without touching ProtagonistEcologyRuntime again.
class BorderlandEvaluator {
public:
    // Walk every genome in `triple_world.borderland().pools_by_archetype[archetype]`
    // and update its stashed fitness via TripleWorldRuntime::evaluateBorderland.
    // Returns the number of genomes evaluated.
    //
    // PLACEHOLDER fitness function (commit D): genome complexity scaled
    // by 100. Commit E will replace this with a real borderland World2D
    // mini-tick evaluation.
    // Commit F: optional `background_archetypes` lets the caller map
    // archetype index N (where N >= 1) to a concrete SpeciesArchetype
    // enum so the evaluator can call makeBackgroundAgent for that
    // species. background_archetypes[N-1] is consulted; if nullptr or
    // shorter than N-1, that archetype falls back to the
    // commit-D placeholder fitness. archetype=0 (protagonist) ignores
    // this argument and always goes through the protagonist mini-tick.
    static std::size_t evaluatePool(
        TripleWorldRuntime& triple_world,
        std::size_t archetype,
        const ProtagonistEcologyConfig& route,
        double aging_factor = 1.0,
        const std::vector<SpeciesArchetype>* background_archetypes = nullptr);

    // Commit J/K: applies sandbox-NEAT-style weight mutation to every
    // genome in the archetype's borderland pool BEFORE the next
    // evaluatePool runs. Per weight:
    //   Bernoulli(reset_rate)  -> reset to N(0,1)
    //   else Bernoulli(perturb_rate) -> perturb by N(0, sigma)
    // Default rates (1.0/0.0) keep commit J's "always perturb"
    // semantics so existing toml configs are unchanged. Setting all
    // three to 0 is a no-op. ProtagonistGenome targets its dense
    // weight blocks; NeatGenome targets enabled ConnectionGene::weight.
    static std::size_t mutatePool(TripleWorldRuntime& triple_world,
                                  std::size_t archetype,
                                  double sigma,
                                  std::uint32_t seed,
                                  double perturb_rate = 1.0,
                                  double reset_rate = 0.0);

    // Commit L: structural NEAT mutation on the borderland pool. For
    // each NeatGenome the per-genome Bernoulli(add_conn_rate) decides
    // whether to attempt an add-connection mutation, and Bernoulli(
    // add_node_rate) whether to attempt add-node (split an enabled
    // edge). Mirrors sandbox NeatEvolution::addConnectionMutation /
    // addNodeMutation with a local InnovationTracker that is seeded
    // from the highest NodeId present in the pool so new node ids
    // never collide with existing ones. ProtagonistGenome (dense
    // weight blocks) has no structural concept and is silently
    // skipped — borderland still keeps protagonists, they just don't
    // receive structural mutations. Rates default to 0 (off) so
    // existing configs are unchanged. Returns the number of genomes
    // for which the mutator lambda was invoked (matches mutatePool's
    // semantics — it counts every genome visited, including
    // ProtagonistGenome entries where the mutator is a no-op). The
    // underlying helper retries up to 24 times for add-connection but
    // may still fail in a fully-connected graph; that is not surfaced
    // in the return value.
    static std::size_t mutateStructuralPool(TripleWorldRuntime& triple_world,
                                            std::size_t archetype,
                                            double add_conn_rate,
                                            double add_node_rate,
                                            std::uint32_t seed,
                                            float tau_min = 0.5f,
                                            float tau_max = 5.0f);
};

}  // namespace neuro::routes::protagonist
