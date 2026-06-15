#include "runtime/BorderlandEvaluator.h"

#include "brain/ProtagonistGenome.h"
#include "core/agent/Agent.h"
#include "core/brain/neat/InnovationTracker.h"
#include "core/brain/neat/NeatGenome.h"
#include "core/interfaces/IGenome.h"
#include "core/world/World2D.h"
#include "core/world/layers/AgentLayer.h"
#include "runtime/BootstrapPopulation.h"
#include "runtime/ProtagonistWorldFactory.h"
#include "world/BaseLayer.h"

#include <algorithm>
#include <memory>
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
// L6 R3 stage 3 commit G: protagonist mini-world peer count.
// Borderland eval was originally a 1-agent isolated world (commit E);
// commit G injects up to N peer protagonists drawn from the same
// archetype's borderland pool so cooperation / signal / co-presence
// reward paths actually fire. Peers are not the agent under
// evaluation — fitness extraction still tracks the focal agent only.
constexpr std::size_t kBorderlandPeerCount = 3;

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

double miniTickProtagonist(const IGenome& g,
                           const ProtagonistEcologyConfig& route,
                           const WorldPopulationPool& pool) {
    auto world = createBootstrapWorld(route);
    if (!world) return 0.0;
    auto* agent_layer = world->getLayer<AgentLayer>();
    if (agent_layer == nullptr) return 0.0;

    // Inject the focal genome as agent_id=1.
    std::mt19937 rng(static_cast<std::uint32_t>(g.id().value));
    auto eval_owned = makeProtagonistAgent(
        AgentId{1u}, g, route, rng, world->getLayer<BaseLayer>());
    if (!eval_owned) return 0.0;
    Agent* eval_agent = eval_owned.get();
    agent_layer->addAgent(std::move(eval_owned), *world);

    // L6 R3 stage 3 commit G: inject up to kBorderlandPeerCount peers
    // from the same borderland pool so cooperation / signal /
    // co-presence reward paths can actually fire. Peers use the same
    // makeProtagonistAgent path so their capability stack is identical;
    // the focal agent is distinguished only by being the one whose
    // fitness we read at the end.
    std::uint64_t next_id_value = 2u;
    std::size_t peers_added = 0;
    for (const auto& peer_g : pool.genomes) {
        if (peers_added >= kBorderlandPeerCount) break;
        if (!peer_g) continue;
        if (peer_g->id().value == g.id().value) continue;  // skip self
        std::mt19937 peer_rng(static_cast<std::uint32_t>(peer_g->id().value + 7919u));
        auto peer_owned = makeProtagonistAgent(
            AgentId{next_id_value}, *peer_g, route, peer_rng,
            world->getLayer<BaseLayer>());
        if (peer_owned) {
            agent_layer->addAgent(std::move(peer_owned), *world);
            ++peers_added;
            ++next_id_value;
        }
    }
    return tickAndScore(*world, *eval_agent);
}

// Commit O: meat-eating archetypes that benefit from a LargeHerbivore
// prey peer in the borderland mini-tick. Strict predators (commit N
// triggers via isPredatorArchetype) plus Omnivore — Omnivore is a
// real meat-eater with a hunt capability, so a herbivore peer is a
// valid evolutionary target for it too. Scavenger deliberately stays
// off this list for now: scavengers feed on carrion (dead bodies),
// which a live herbivore peer doesn't supply within a 100-substep
// mini-tick. Adding a corpse spawner is a separate commit.
bool benefitsFromPreyPeer(SpeciesArchetype a) {
    return isPredatorArchetype(a)
        || a == SpeciesArchetype::Omnivore;
}

// Commit F: same shape but for background species. The caller supplies
// the concrete SpeciesArchetype so makeBackgroundAgent picks the right
// capability mix. Returns 0.0 if the agent can't be built (e.g. a
// non-NeatGenome leaked into the borderland pool).
//
// Commit N: per-archetype eval flavor. When the focal archetype is a
// predator (ApexPredator / PackHunter / AmbushPredator), inject 1
// prey peer drawn from `prey_pool` so the mini-tick has a real
// hunting target. Without this, predator borderland selection just
// rewards "stay alive in an empty world" — every predator scores
// the same and add_conn / add_node mutation has no traction. The
// prey is picked from the top of `prey_pool` (highest stashed
// fitness, which promoteExternalToBorderland sorts first). Falls
// back silently if prey_pool is null / empty (e.g. early
// generations before the LargeHerbivore borderland is populated),
// or if makeBackgroundAgent throws (genome layout mismatch).
//
// Commit O: extended trigger to also fire for Omnivore. Scavenger is
// excluded (needs carrion not live prey) and stays at the commit-F
// empty-world default until a corpse spawner is wired.
double miniTickBackground(const IGenome& g,
                          SpeciesArchetype archetype,
                          const ProtagonistEcologyConfig& route,
                          const std::vector<std::unique_ptr<IGenome>>* prey_pool = nullptr,
                          SpeciesArchetype prey_archetype = SpeciesArchetype::LargeHerbivore) {
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

    // Commit N: prey peer injection (predators); commit O: + Omnivore.
    if (prey_pool != nullptr && !prey_pool->empty() && benefitsFromPreyPeer(archetype)) {
        // Walk the pool top-down for the first non-null genome.
        // Pools are append-only (commit B/F protocol) so position 0
        // is the earliest entry; promoteExternalToBorderland inserts
        // top-fitness first which keeps the same ordering across
        // borderland mutate+eval generations.
        const IGenome* prey_g = nullptr;
        for (const auto& p : *prey_pool) {
            if (p) { prey_g = p.get(); break; }
        }
        if (prey_g != nullptr) {
            std::mt19937 prey_rng(static_cast<std::uint32_t>(prey_g->id().value + 31337u));
            try {
                auto prey_owned = makeBackgroundAgent(
                    AgentId{2u}, prey_archetype, *prey_g, route, prey_rng);
                if (prey_owned) agent_layer->addAgent(std::move(prey_owned), *world);
            } catch (const std::exception&) {
                // Prey genome layout mismatch — silently skip; predator
                // mini-tick still runs without a target.
            }
        }
    }
    return tickAndScore(*world, *agent);
}

double placeholderFitness(const IGenome& g) {
    return static_cast<double>(g.complexity()) * 100.0;
}

// Commit J/K: in-place Gaussian weight perturbation matching the
// sandbox NEAT pattern (NeatEvolution::mutateWeights):
//   per weight: Bernoulli(reset_rate) -> reset to N(0,1),
//               else Bernoulli(perturb_rate) -> perturb by N(0,sigma).
// Default rates (1.0/0.0) reproduce commit J's "always perturb" behavior
// so existing configs unchanged. Setting both to 0 is a no-op. Dispatches
// on concrete genome type — IGenome has no uniform mutate API.
void perturbGenomeWeights(IGenome& g,
                          std::mt19937& rng,
                          double sigma,
                          double perturb_rate,
                          double reset_rate) {
    if (sigma <= 0.0 && reset_rate <= 0.0) return;
    std::bernoulli_distribution perturb(perturb_rate);
    std::bernoulli_distribution reset(reset_rate);
    std::normal_distribution<float> noise(0.0f, static_cast<float>(sigma));
    std::normal_distribution<float> reset_dist(0.0f, 1.0f);

    auto perturb_one = [&](float& w) {
        if (reset_rate > 0.0 && reset(rng)) {
            w = reset_dist(rng);
        } else if (sigma > 0.0 && perturb(rng)) {
            w += noise(rng);
        }
    };

    if (auto* pg = dynamic_cast<ProtagonistGenome*>(&g)) {
        for (auto& w : pg->inputHiddenWeights())     perturb_one(w);
        for (auto& w : pg->hiddenActionWeights())    perturb_one(w);
        for (auto& w : pg->hiddenInterfaceWeights()) perturb_one(w);
        return;
    }
    if (auto* ng = dynamic_cast<NeatGenome*>(&g)) {
        for (auto& c : ng->connections()) {
            if (c.enabled) perturb_one(c.weight);
        }
        return;
    }
    // Other genome types (none yet) silently skip — no IGenome-level
    // mutation API exists, and we don't want to throw inside a hot loop.
}

// Commit L: scan a NEAT genome and report the highest NodeId so the
// shared InnovationTracker can be seeded past it (otherwise the very
// first nextNodeId() the tracker hands out would collide with an
// already-existing node). The pool's tracker is created once per
// mutateStructuralPool call and bumped past the max across the whole
// pool so a borderland with mixed-history genomes doesn't accidentally
// reuse a NodeId that already lives in some other pool member.
NodeId maxNodeIdInGenome(const NeatGenome& g) {
    NodeId mx{0};
    for (const auto& n : g.nodes()) {
        if (static_cast<std::uint64_t>(n.id) > static_cast<std::uint64_t>(mx)) mx = n.id;
    }
    return mx;
}

// Commit L: add-connection mutation. Mirrors
// NeatEvolution::addConnectionMutation in sandbox NEAT. Up to 24
// random (source, target) attempts; skip if target is Input or the
// edge already exists; otherwise push a new ConnectionGene with a
// fresh innovation id from the supplied tracker.
//
// `tracker` is shared across all genomes in this mutateStructuralPool
// call so a borderland generation receives consistent innovation ids
// (the same as how sandbox NeatEvolution::beginGeneration would
// behave). We don't call beginGeneration() here because the sandbox
// tracker's generation cache is used to dedupe identical (from, to)
// edges added across genomes in the same generation; we don't have
// access to that cache for the borderland tracker, but
// nextConnectionInnovation already records the cache itself.
void addConnectionMutationNeat(NeatGenome& genome,
                               InnovationTracker& tracker,
                               std::mt19937& rng) {
    if (genome.nodes().size() < 2) return;
    std::uniform_int_distribution<std::size_t> source_dist(0, genome.nodes().size() - 1);
    std::uniform_int_distribution<std::size_t> target_dist(0, genome.nodes().size() - 1);
    for (std::size_t attempt = 0; attempt < 24; ++attempt) {
        const auto& source = genome.nodes()[source_dist(rng)];
        const auto& target = genome.nodes()[target_dist(rng)];
        if (target.type == NodeGeneType::Input) continue;
        if (genome.hasConnection(source.id, target.id)) continue;
        std::normal_distribution<float> weight_dist(0.0f, 1.0f);
        genome.connections().push_back(ConnectionGene{
            source.id,
            target.id,
            weight_dist(rng),
            true,
            tracker.nextConnectionInnovation(source.id, target.id),
        });
        return;
    }
}

// Commit L: add-node mutation. Mirrors
// NeatEvolution::addNodeMutation. Splits a random enabled edge into
// two: disable the edge, insert a new hidden node, push two edges
// (source->new with weight 1, new->target with the old weight).
// `tracker` allocates the new NodeId; it must be bumped past the
// pool's existing max NodeId before this is called (mutateStructuralPool
// does that once per call).
void addNodeMutationNeat(NeatGenome& genome,
                         InnovationTracker& tracker,
                         std::mt19937& rng,
                         float tau_min,
                         float tau_max) {
    std::vector<std::size_t> enabled_indices;
    for (std::size_t i = 0; i < genome.connections().size(); ++i) {
        if (genome.connections()[i].enabled) enabled_indices.push_back(i);
    }
    if (enabled_indices.empty()) return;

    std::uniform_int_distribution<std::size_t> index_dist(0, enabled_indices.size() - 1);
    const std::size_t split_index = enabled_indices[index_dist(rng)];
    const ConnectionGene split = genome.connections()[split_index];
    genome.connections()[split_index].enabled = false;

    const NodeId new_node_id = tracker.nextNodeId();
    std::normal_distribution<float> bias_dist(0.0f, 1.0f);
    std::uniform_real_distribution<float> tau_dist(tau_min, tau_max);
    genome.nodes().push_back(NodeGene{
        new_node_id,
        NodeGeneType::Hidden,
        bias_dist(rng),
        1.0f,
        tau_dist(rng),
    });
    genome.connections().push_back(ConnectionGene{
        split.from,
        new_node_id,
        1.0f,
        true,
        tracker.nextConnectionInnovation(split.from, new_node_id),
    });
    genome.connections().push_back(ConnectionGene{
        new_node_id,
        split.to,
        split.weight,
        true,
        tracker.nextConnectionInnovation(new_node_id, split.to),
    });
}

}  // namespace

std::size_t BorderlandEvaluator::evaluatePool(
    TripleWorldRuntime& triple_world,
    std::size_t archetype,
    const ProtagonistEcologyConfig& route,
    double aging_factor,
    const std::vector<SpeciesArchetype>* background_archetypes) {
    if (archetype == 0) {
        // Commit G: capture the borderland pool by reference so the
        // mini-world can inject peer protagonists drawn from the same
        // pool. Pool is mutated by evaluateBorderland (it updates
        // fitness slots), but our read of pool.genomes happens before
        // each per-genome write so iteration stability is fine.
        const auto& proto_pool = triple_world.borderland().pools_by_archetype.at(archetype);
        return triple_world.evaluateBorderland(
            archetype,
            /*eval_fn=*/[&route, &proto_pool](const IGenome& g) {
                return miniTickProtagonist(g, route, proto_pool);
            },
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
        // Commit N: predators get a LargeHerbivore prey peer drawn
        // from the herbivore borderland pool. Commit O: also fires
        // for Omnivore. We look up the pool index by walking
        // background_archetypes for LargeHerbivore; if it's not in
        // this run's background mix, prey_pool stays null and
        // miniTickBackground falls back to the empty mini-world
        // (commit F default).
        const std::vector<std::unique_ptr<IGenome>>* prey_pool = nullptr;
        if (benefitsFromPreyPeer(sa)) {
            for (std::size_t i = 0; i < background_archetypes->size(); ++i) {
                if ((*background_archetypes)[i] == SpeciesArchetype::LargeHerbivore) {
                    const std::size_t herb_idx = i + 1;  // archetype index = bg_index+1
                    const auto& herb_pool = triple_world.borderland().pools_by_archetype.at(herb_idx);
                    if (!herb_pool.genomes.empty()) prey_pool = &herb_pool.genomes;
                    break;
                }
            }
        }
        return triple_world.evaluateBorderland(
            archetype,
            /*eval_fn=*/[&route, sa, prey_pool](const IGenome& g) {
                return miniTickBackground(g, sa, route, prey_pool);
            },
            aging_factor);
    }
    return triple_world.evaluateBorderland(
        archetype,
        /*eval_fn=*/[](const IGenome& g) { return placeholderFitness(g); },
        aging_factor);
}

std::size_t BorderlandEvaluator::mutatePool(TripleWorldRuntime& triple_world,
                                            std::size_t archetype,
                                            double sigma,
                                            std::uint32_t seed,
                                            double perturb_rate,
                                            double reset_rate) {
    if (sigma <= 0.0 && reset_rate <= 0.0) return 0;
    auto rng = std::make_shared<std::mt19937>(seed == 0u ? 1u : seed);
    return triple_world.mutateBorderlandPool(
        archetype,
        /*mutator=*/[rng, sigma, perturb_rate, reset_rate](IGenome& g) {
            perturbGenomeWeights(g, *rng, sigma, perturb_rate, reset_rate);
        });
}

std::size_t BorderlandEvaluator::mutateStructuralPool(
    TripleWorldRuntime& triple_world,
    std::size_t archetype,
    double add_conn_rate,
    double add_node_rate,
    std::uint32_t seed,
    float tau_min,
    float tau_max) {
    if (add_conn_rate <= 0.0 && add_node_rate <= 0.0) return 0;

    // Pre-scan the pool to seed the InnovationTracker past every
    // existing NodeId. Without this, tracker.nextNodeId() would start
    // at 1 and immediately collide with input/output nodes inserted
    // when the genomes were first minted.
    auto& pool = triple_world.borderland().pools_by_archetype.at(archetype);
    NodeId pool_max_node{0};
    for (const auto& g : pool.genomes) {
        if (!g) continue;
        const auto* ng = dynamic_cast<const NeatGenome*>(g.get());
        if (ng == nullptr) continue;
        const NodeId mx = maxNodeIdInGenome(*ng);
        if (static_cast<std::uint64_t>(mx) > static_cast<std::uint64_t>(pool_max_node)) {
            pool_max_node = mx;
        }
    }
    auto tracker = std::make_shared<InnovationTracker>();
    tracker->ensureNextNodeIdAtLeast(NodeId{static_cast<std::uint64_t>(pool_max_node) + 1u});

    auto rng = std::make_shared<std::mt19937>(seed == 0u ? 1u : seed);
    return triple_world.mutateBorderlandPool(
        archetype,
        /*mutator=*/[rng, tracker, add_conn_rate, add_node_rate, tau_min, tau_max](IGenome& g) {
            auto* ng = dynamic_cast<NeatGenome*>(&g);
            if (ng == nullptr) return;  // ProtagonistGenome (dense) — skip
            std::bernoulli_distribution add_conn(add_conn_rate);
            std::bernoulli_distribution add_node(add_node_rate);
            if (add_conn_rate > 0.0 && add_conn(*rng)) {
                addConnectionMutationNeat(*ng, *tracker, *rng);
            }
            if (add_node_rate > 0.0 && add_node(*rng)) {
                addNodeMutationNeat(*ng, *tracker, *rng, tau_min, tau_max);
            }
        });
}

}  // namespace neuro::routes::protagonist
