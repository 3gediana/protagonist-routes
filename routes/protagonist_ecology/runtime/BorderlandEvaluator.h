#pragma once

#include "runtime/ProtagonistEcologyConfig.h"
#include "runtime/TripleWorldRuntime.h"

#include <cstddef>

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
    static std::size_t evaluatePool(TripleWorldRuntime& triple_world,
                                    std::size_t archetype,
                                    const ProtagonistEcologyConfig& route,
                                    double aging_factor = 1.0);
};

}  // namespace neuro::routes::protagonist
