#include "runtime/BorderlandEvaluator.h"

#include "core/interfaces/IGenome.h"

namespace neuro::routes::protagonist {

// L6 R3 stage 3 commit D placeholder fitness: scales the genome's
// complexity by 100. This is intentionally trivial — the goal of
// commit D is to lock in the call-site shape so commit E only has
// to swap this implementation. Real evaluation (commit E) will
// build a minimal World2D, inject the genome as a protagonist
// agent, tick N steps, and extract episode-end fitness.
namespace {
double placeholderFitness(const IGenome& g) {
    return static_cast<double>(g.complexity()) * 100.0;
}
}  // namespace

std::size_t BorderlandEvaluator::evaluatePool(TripleWorldRuntime& triple_world,
                                              std::size_t archetype,
                                              const ProtagonistEcologyConfig& route,
                                              double aging_factor) {
    (void)route;  // unused in placeholder; commit E will use it for World2D config
    return triple_world.evaluateBorderland(
        archetype,
        /*eval_fn=*/[](const IGenome& g) { return placeholderFitness(g); },
        aging_factor);
}

}  // namespace neuro::routes::protagonist
