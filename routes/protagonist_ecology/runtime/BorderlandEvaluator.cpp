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

// Walk a 1-agent mini-world for the configured number of substeps and
// return a scalar fitness. Current scoring is the count of substeps the
// agent stayed alive plus its accumulated route-side fitness — both
// are stable signals against the trivial baseline of "agent does
// nothing". Nothing fancy: this is the first commit that actually
// ticks a World2D for borderland eval, so the evolutionary signal
// just has to be (a) non-trivial, (b) reproducible.
double tickAndScore(World2D& world, Agent& agent) {
    std::size_t alive_substeps = 0;
    for (std::size_t i = 0; i < kBorderlandMiniTicks; ++i) {
        world.tick(kBorderlandMiniDt);
        if (agent.alive()) ++alive_substeps;
    }
    return static_cast<double>(alive_substeps) + agent.fitness();
}

double miniTickProtagonist(const IGenome& g, const ProtagonistEcologyConfig& route) {
    auto world = createBootstrapWorld(route);
    if (!world) return 0.0;
    auto* agent_layer = world->getLayer<AgentLayer>();
    if (agent_layer == nullptr) return 0.0;

    std::mt19937 rng(static_cast<std::uint32_t>(g.id().value));
    auto agent_owned = makeProtagonistAgent(
        AgentId{1u}, g, route, rng, world->getLayer<BaseLayer>());
    if (!agent_owned) return 0.0;
    Agent* agent = agent_owned.get();
    agent_layer->addAgent(std::move(agent_owned), *world);
    return tickAndScore(*world, *agent);
}

// Commit F: same shape but for background species. The caller supplies
// the concrete SpeciesArchetype so makeBackgroundAgent picks the right
// capability mix. Returns 0.0 if the agent can't be built (e.g. a
// non-NeatGenome leaked into the borderland pool).
double miniTickBackground(const IGenome& g,
                          SpeciesArchetype archetype,
                          const ProtagonistEcologyConfig& route) {
    auto world = createBootstrapWorld(route);
    if (!world) return 0.0;
    auto* agent_layer = world->getLayer<AgentLayer>();
    if (agent_layer == nullptr) return 0.0;

    std::mt19937 rng(static_cast<std::uint32_t>(g.id().value));
    std::unique_ptr<Agent> agent_owned;
    try {
        agent_owned = makeBackgroundAgent(AgentId{1u}, archetype, g, route, rng);
    } catch (const std::exception&) {
        return 0.0;  // wrong genome layout -> fall back to zero, caller still gets 0 evaluated bump
    }
    if (!agent_owned) return 0.0;
    Agent* agent = agent_owned.get();
    agent_layer->addAgent(std::move(agent_owned), *world);
    return tickAndScore(*world, *agent);
}

double placeholderFitness(const IGenome& g) {
    return static_cast<double>(g.complexity()) * 100.0;
}

}  // namespace

std::size_t BorderlandEvaluator::evaluatePool(
    TripleWorldRuntime& triple_world,
    std::size_t archetype,
    const ProtagonistEcologyConfig& route,
    double aging_factor,
    const std::vector<SpeciesArchetype>* background_archetypes) {
    if (archetype == 0) {
        return triple_world.evaluateBorderland(
            archetype,
            /*eval_fn=*/[&route](const IGenome& g) { return miniTickProtagonist(g, route); },
            aging_factor);
    }
    // Commit F: archetype N (N>=1) maps to background_archetypes[N-1]
    // when the caller supplied the mapping. Without the mapping we
    // can't pick the right capability mix, so fall back to the
    // commit-D complexity placeholder. archetype 0 path above is
    // unaffected.
    if (background_archetypes != nullptr
        && archetype >= 1
        && (archetype - 1) < background_archetypes->size()) {
        const SpeciesArchetype sa = (*background_archetypes)[archetype - 1];
        return triple_world.evaluateBorderland(
            archetype,
            /*eval_fn=*/[&route, sa](const IGenome& g) { return miniTickBackground(g, sa, route); },
            aging_factor);
    }
    return triple_world.evaluateBorderland(
        archetype,
        /*eval_fn=*/[](const IGenome& g) { return placeholderFitness(g); },
        aging_factor);
}

}  // namespace neuro::routes::protagonist
