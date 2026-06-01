#pragma once

// routes/protagonist_ecology/world/PopulationLayer.h
//
// FINAL_BLUEPRINT.md Layer 4: lifecycle + reproduction + lineage.
//
// Responsibilities:
//   1. Decrement per-agent reproduction_cooldown each tick.
//   2. Detect natural death (age > max_age_seconds) and call
//      Agent::killWithCause(OldAge).
//   3. Apply Elder-phase extra mortality (small per-tick chance).
//   4. Pair adjacent same-species adults that meet energy + cooldown
//      requirements and request the SpawnCallback to create offspring.
//   5. Track deaths by DeathCause and total births for diagnostics.
//
// PopulationLayer lives in the route layer (not src/core) because it
// must call the route-specific BootstrapPopulation helpers (which
// know how to wire perceptions/capabilities for each archetype) via
// an injected SpawnCallback.
//
// Design choice: in-world reproduction here uses asexual cloning of
// the better-fit parent's genome with small Gaussian weight noise.
// True NEAT crossover (with topology / innovation merging) is reserved
// for the cross-episode evolution loop. This keeps the in-world
// reproduction cheap, deterministic, and side-effect free w.r.t. the
// cross-episode innovation tracker.

#include "core/agent/Agent.h"
#include "core/interfaces/IWorldLayer.h"
#include "core/types/Aliases.h"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <vector>

namespace neuro {
class IGenome;
}

namespace neuro::routes::protagonist {

// Per-episode born child record. PopulationLayer accumulates one of these
// per successful in-world birth; the runtime drains the list at the end
// of the episode and injects matching-archetype entries into the next
// generation's evolution pool (so in-world reproduction is not just
// visual but actually contributes to the cross-episode NEAT search).
struct BornChild {
    std::uint8_t archetype_tag = 0;
    GenomeId genome_id{0};
    AgentId  agent_id{0};
    std::unique_ptr<IGenome> genome_clone;  // separate copy for evolution
};

class PopulationLayer final : public IWorldLayer {
public:
    static constexpr const char* kName = "population";

    // Spawn callback. PopulationLayer hands over both parents and the
    // already-allocated AgentId/GenomeId. The callback must:
    //   - clone parent_a's genome, apply small mutation
    //   - construct a new Agent with proper perceptions/capabilities
    //     for parent_a's species archetype
    //   - mark Agent as juvenile (age=0), set reproduction_cooldown to a
    //     sentinel large value (so juveniles can't immediately breed)
    //   - return the new Agent (PopulationLayer will register it)
    // Returning nullptr signals "skip this pair" (e.g. archetype not
    // supported for in-world breeding).
    using SpawnCallback = std::function<std::unique_ptr<Agent>(
        const Agent& parent_a,
        const Agent& parent_b,
        AgentId new_agent_id,
        GenomeId new_genome_id)>;

    struct Coefficients {
        // Pair-finding cadence. We don't need to scan every tick; sec=5
        // is fine and keeps O(N^2) cost low.
        SimTimeSeconds reproduction_check_interval = 5.0;

        // Distance (world units) for pair eligibility.
        float reproduction_radius = 8.0f;

        // Energy thresholds.
        float reproduction_min_energy   = 60.0f;
        float reproduction_energy_cost  = 25.0f;
        float juvenile_initial_energy   = 30.0f;
        // After successful reproduction, both parents enter cooldown.
        float reproduction_cooldown_sec = 90.0f;
        // Juveniles also start in cooldown (cannot reproduce until adult).
        float juvenile_initial_cooldown_sec = 200.0f;

        // Elder extra mortality: per-second probability of dying of "Disease"
        // (proxy for general aging frailty) once in Elder stage.
        float elder_extra_mortality_per_sec = 0.0005f;

        // Population guardrail: don't breed past this many living agents
        // (cluster total, all species). Prevents pathological growth in
        // training when reward shaping accidentally makes life cheap.
        std::size_t max_living_agents = 600;

        // Random seed (0 => from random_device).
        std::uint32_t seed = 0;

        // Throttle: max number of spawns per reproduction tick. Each
        // makeChildFromParents call costs ~100ms (NeatBrain reconstruction);
        // spawning many in one tick stalls the world for seconds.
        // 1 = at most 1 birth per check, fully amortized.
        std::size_t max_spawns_per_tick = 1;
    };

    PopulationLayer(Coefficients coeffs, SpawnCallback spawn_cb);

    const char* layerName() const override { return kName; }
    void onAttach(IWorld& world) override;
    void tick(IWorld& world, float dt) override;

    // ---- Diagnostics ----
    std::size_t totalBirths() const { return total_births_; }
    std::size_t totalDeaths() const { return total_deaths_; }
    std::size_t deathsByCause(Agent::DeathCause c) const;
    SimTimeSeconds simTime() const { return sim_time_; }

    // ---- Cross-episode bridge ----
    // Drain accumulated born-child genome clones so the runtime can inject
    // them into the next generation's evolution pool. Returns ownership;
    // the layer's internal list is emptied. Safe to call multiple times.
    std::vector<BornChild> drainBornChildren();

    // Reset stats (used on episode reset).
    void resetStatistics();

    // Manual ID injection (so the runtime can keep IDs unique across
    // bootstrap-seeded agents). PopulationLayer will start allocating
    // from these values when births occur.
    void setNextIds(std::uint64_t agent_id, std::uint64_t genome_id, std::uint64_t lineage_id);

private:
    Coefficients coeffs_;
    SpawnCallback spawn_cb_;
    std::mt19937 rng_;

    SimTimeSeconds sim_time_ = 0.0;
    SimTimeSeconds last_check_time_ = -1.0e9;

    std::uint64_t next_agent_id_ = 1'000'000;
    std::uint64_t next_genome_id_ = 1'000'000;
    std::uint64_t next_lineage_id_ = 1;

    std::size_t total_births_ = 0;
    std::size_t total_deaths_ = 0;
    std::array<std::size_t, 9> deaths_by_cause_{};

    // L1 #1 cross-episode reproduction bridge. Cleared on resetStatistics()
    // and drained externally via drainBornChildren().
    std::vector<BornChild> born_children_;
};

}  // namespace neuro::routes::protagonist
