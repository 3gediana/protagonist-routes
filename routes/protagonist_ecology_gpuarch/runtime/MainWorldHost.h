#pragma once

#include "core/types/Aliases.h"
#include "runtime/BootstrapPopulation.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <random>

namespace neuro {
class IGenome;
class World2D;
}

namespace neuro::routes::protagonist {

struct ProtagonistEcologyConfig;

// Layer 6 R3 stage 4 (LAYER6_THREE_WORLDS_SPEC § 1.3 + § 2.5,
// PRODUCTION_WORLD_LIFECYCLE_SPEC invariant #1): persistent main-world
// host. Owns a single World2D that is created once at host
// construction and NEVER reset between training generations. This
// realizes the "主世界永不 reset" invariant.
//
// Scope of stage 4 (matches the spec § 6 priority-2 goal "MainWorld
// 不 reset 但内存中"):
//   - own one World2D instance via createBootstrapWorld
//   - tick(dt) advances physics without ever recreating the world
//   - the underlying World2D itself tracks the cumulative sim_time
//     and tick counter, so monotonicity falls out automatically
//
// Stage 4b extension (this commit) — live agent seeding from the
// borderland→main_world promotion pipeline:
//   - seedAgent(archetype, genome, lineage_id, config) installs one
//     agent into the host's persistent World2D using the same
//     BootstrapPopulation helpers the sandbox world uses, so the
//     spawned agent has its full perception/capability wiring
//   - seeded agents persist across ticks because the host's World2D
//     never resets; they age via the existing LifecycleLayer and
//     can reproduce via the existing PopulationLayer just like
//     bootstrap-spawned agents in the sandbox
//   - the host owns its own deterministic mt19937 so repeated calls
//     across the run produce a stable agent-id / spawn-position
//     sequence for a given route.random_seed
//
// Out of scope (explicitly deferred per spec § 6 priority-3 "序列化到 disk"):
//   - serialize the World2D state to disk
//   - reload after process restart
//
// Out of scope (R3 stage 5 and beyond, to be wired in follow-up commits):
//   - in-world reproduction loop driving fresh lineages
//   - fitness-window snapshots
//
// The host is intentionally tiny so that it can be unit-tested
// without dragging in the rest of the training runtime.
class MainWorldHost {
public:
    // Constructs a fresh persistent main world from `route`. The
    // ProtagonistEcologyConfig must outlive this host only for the
    // duration of the constructor; the host does not store the config
    // (it stashes everything it needs into the World2D directly via
    // createBootstrapWorld). The host's internal RNG is seeded
    // deterministically from `route.random_seed`.
    explicit MainWorldHost(const ProtagonistEcologyConfig& route);
    ~MainWorldHost();

    MainWorldHost(const MainWorldHost&) = delete;
    MainWorldHost& operator=(const MainWorldHost&) = delete;
    MainWorldHost(MainWorldHost&&) = delete;
    MainWorldHost& operator=(MainWorldHost&&) = delete;

    // Advance the persistent main world by dt_seconds of simulated
    // time. Negative / zero dt is a no-op (returns false). The world
    // is NEVER recreated by this call; subsequent calls always
    // continue from the prior state — that is the "永不 reset"
    // invariant. Returns true if the tick actually ran.
    bool tick(SimTimeSeconds dt_seconds);

    // Cumulative simulated seconds advanced via tick(). Sourced from
    // the underlying World2D's own monotonic clock so we don't risk
    // double-bookkeeping. Returns 0 if the world failed to construct.
    SimTimeSeconds simTimeSeconds() const;

    // Number of successful tick() calls (dt_seconds > 0) that have
    // run against the world.
    std::size_t tickCallCount() const { return tick_call_count_; }

    // L6 R3 stage 4b: install one agent into the persistent main
    // world from a borderland→main_world promoted genome.
    //
    // The agent is constructed via the BootstrapPopulation typed
    // helpers (makeProtagonistAgent for archetype==Protagonist,
    // makeBackgroundAgent otherwise) using the host's own RNG so
    // spawn jitter is deterministic across runs with the same
    // route.random_seed. The agent is then stamped with the supplied
    // `lineage_id` and added to the AgentLayer of the host's World2D.
    //
    // Returns true on success. Returns false on hard failures (the
    // host has no World2D, the World2D has no AgentLayer, or the
    // agent factory returned nullptr).
    //
    // Note: seedAgent does NOT consume the genome; the caller still
    // owns it. The agent is built from the genome by clone/build
    // inside the factory helper, so the borderland/main pool
    // ownership is preserved for subsequent re-evaluation or rescue
    // flows.
    bool seedAgent(SpeciesArchetype archetype,
                   const IGenome& genome,
                   std::uint64_t lineage_id,
                   const ProtagonistEcologyConfig& config);

    // Number of successful seedAgent() calls since construction.
    // Agents added this way may die later via the lifecycle layer
    // — seedCount is the cumulative spawn count, NOT the current
    // alive count. Use world()->getLayer<AgentLayer>()->livingCount()
    // for the live tally.
    std::size_t seedCount() const { return seed_count_; }

    // L6 R3 stage 4c (end-of-training main_world freeze snapshot,
    // MVP): dump every alive agent currently in the host's persistent
    // World2D into a single JSON manifest at
    // `<dir>/main_world_agents.json`.
    //
    // Each entry records the agent's id, archetype_tag, species_id,
    // genome_id, lineage_id, and (x,y) position. Genome blobs are NOT
    // duplicated here — they are reachable via genome_id which
    // cross-references the existing MainWorldPersistence pool
    // snapshot written alongside this file (see
    // ProtagonistEcologyRuntime end-of-run path).
    //
    // The world's environment state (fires, worksites, resource
    // patches, etc.) is intentionally NOT serialized in this MVP; a
    // future Layer 7 viewer can rebuild a plausible environment from
    // the same toml config that drove the run.
    //
    // Creates `dir` (recursively) if it does not exist. Returns the
    // number of alive agents written; returns 0 on failure (no
    // World2D, no AgentLayer, or filesystem error).
    std::size_t saveAgentsSnapshot(const std::filesystem::path& dir) const;

    // Direct access to the owned World2D for callers that need to
    // inspect or interact with layers (e.g. tests, future migration
    // hooks). Stable for the host's lifetime.
    World2D* world() { return world_.get(); }
    const World2D* world() const { return world_.get(); }

private:
    std::unique_ptr<World2D> world_;
    std::size_t tick_call_count_{0};
    std::size_t seed_count_{0};
    std::uint64_t next_agent_id_{1};
    std::mt19937 rng_;
};

}  // namespace neuro::routes::protagonist
