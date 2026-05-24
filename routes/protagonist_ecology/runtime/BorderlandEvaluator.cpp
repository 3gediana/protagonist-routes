#include "runtime/BorderlandEvaluator.h"

#include "core/agent/Agent.h"
#include "core/interfaces/IGenome.h"
#include "core/world/World2D.h"
#include "core/world/layers/AgentLayer.h"
#include "runtime/BootstrapPopulation.h"
#include "runtime/ProtagonistWorldFactory.h"
#include "world/BaseLayer.h"

#include <random>

namespace neuro::routes::protagonist {

namespace {

// Number of substeps the borderland mini-evaluation ticks per genome.
// 100 substeps * 0.1 sim sec = 10 sim sec; full sandbox episode is
// 4900 substeps (~490 sim sec) so this is ~1/49 of an episode.
// Long enough for an agent to interact with resources / drives, short
// enough that evaluating a 32-genome borderland pool stays under a
// few seconds.
constexpr std::size_t kBorderlandMiniTicks = 100;
constexpr float kBorderlandMiniDt = 0.1f;

// Walk the 1-protagonist mini-world for the configured number of
// substeps and return a scalar fitness. Current scoring is the count
// of substeps the agent stayed alive plus its accumulated route-side
// fitness — both are stable signals against the trivial baseline of
// "agent does nothing". Nothing fancy: this is the first commit that
// actually ticks a World2D for borderland eval, so the evolutionary
// signal just has to be (a) non-trivial, (b) reproducible.
double miniTickEvaluate(const IGenome& g, const ProtagonistEcologyConfig& route) {
    auto world = createBootstrapWorld(route);
    if (!world) return 0.0;
    auto* agent_layer = world->getLayer<AgentLayer>();
    if (agent_layer == nullptr) return 0.0;

    // Deterministic per-genome RNG so the same genome always evaluates
    // to the same fitness on identical world layouts.
    std::mt19937 rng(static_cast<std::uint32_t>(g.id().value));
    auto agent_owned = makeProtagonistAgent(
        AgentId{1u}, g, route, rng, world->getLayer<BaseLayer>());
    if (!agent_owned) return 0.0;
    Agent* agent = agent_owned.get();
    agent_layer->addAgent(std::move(agent_owned), *world);

    std::size_t alive_substeps = 0;
    for (std::size_t i = 0; i < kBorderlandMiniTicks; ++i) {
        world->tick(kBorderlandMiniDt);
        if (agent->alive()) ++alive_substeps;
    }
    // Combine "survival ticks" with whatever route-side fitness the
    // capability stack accumulated. Both are non-negative under the
    // current reward design.
    return static_cast<double>(alive_substeps) + agent->fitness();
}

}  // namespace

std::size_t BorderlandEvaluator::evaluatePool(TripleWorldRuntime& triple_world,
                                              std::size_t archetype,
                                              const ProtagonistEcologyConfig& route,
                                              double aging_factor) {
    // Mini-tick path is implemented for archetype 0 (protagonist) only:
    // makeProtagonistAgent expects a ProtagonistGenome layout. Background
    // archetypes 1..8 fall back to the commit-D placeholder until their
    // own makeBackgroundAgent path is wired (deferred to a later commit).
    if (archetype == 0) {
        return triple_world.evaluateBorderland(
            archetype,
            /*eval_fn=*/[&route](const IGenome& g) { return miniTickEvaluate(g, route); },
            aging_factor);
    }
    return triple_world.evaluateBorderland(
        archetype,
        /*eval_fn=*/[](const IGenome& g) {
            return static_cast<double>(g.complexity()) * 100.0;
        },
        aging_factor);
}

}  // namespace neuro::routes::protagonist
