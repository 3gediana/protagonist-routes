#include "runtime/MainWorldHost.h"

#include "runtime/BootstrapPopulation.h"
#include "runtime/ProtagonistEcologyConfig.h"
#include "runtime/ProtagonistWorldFactory.h"

#include "core/agent/Agent.h"
#include "core/world/World2D.h"
#include "core/world/layers/AgentLayer.h"
#include "world/BaseLayer.h"

#include <cstdint>

namespace neuro::routes::protagonist {

namespace {
// Fixed offset combined with route.random_seed so the MainWorldHost
// RNG stream is decoupled from any other RNG in the runtime (the
// sandbox seedBootstrapPopulation uses +7000; the per-episode bg
// brains use other offsets). The literal spells "MW" in hex.
constexpr std::uint64_t kMainWorldRngSeedOffset = 0x4D575F31u;
}  // namespace

MainWorldHost::MainWorldHost(const ProtagonistEcologyConfig& route)
    : world_(createBootstrapWorld(route)),
      rng_(static_cast<std::mt19937::result_type>(
          static_cast<std::uint64_t>(route.random_seed) + kMainWorldRngSeedOffset)) {
    // Intentionally empty body: createBootstrapWorld already wires
    // every layer the protagonist runtime expects. We hold onto the
    // instance for the host's lifetime so it can be ticked without
    // ever being torn down — that is the LAYER6_THREE_WORLDS_SPEC
    // § 1.3 "永不 reset" invariant in practice.
}

MainWorldHost::~MainWorldHost() = default;

bool MainWorldHost::tick(SimTimeSeconds dt_seconds) {
    if (dt_seconds <= 0.0) {
        return false;
    }
    if (world_ == nullptr) {
        return false;
    }
    world_->tick(static_cast<float>(dt_seconds));
    ++tick_call_count_;
    return true;
}

SimTimeSeconds MainWorldHost::simTimeSeconds() const {
    if (world_ == nullptr) {
        return 0.0;
    }
    return world_->currentTimeSeconds();
}

bool MainWorldHost::seedAgent(SpeciesArchetype archetype,
                              const IGenome& genome,
                              std::uint64_t lineage_id,
                              const ProtagonistEcologyConfig& config) {
    if (world_ == nullptr) {
        return false;
    }
    auto* agent_layer = world_->getLayer<AgentLayer>();
    if (agent_layer == nullptr) {
        return false;
    }
    const auto* base_layer = world_->getLayer<BaseLayer>();

    const AgentId new_id{next_agent_id_++};
    std::unique_ptr<Agent> agent;
    if (archetype == SpeciesArchetype::Protagonist) {
        agent = makeProtagonistAgent(new_id, genome, config, rng_, base_layer);
    } else {
        agent = makeBackgroundAgent(new_id, archetype, genome, config, rng_);
    }
    if (agent == nullptr) {
        return false;
    }
    stampLifecycleAndLineage(*agent, archetype, config, lineage_id, rng_);
    agent_layer->addAgent(std::move(agent), *world_);
    ++seed_count_;
    return true;
}

}  // namespace neuro::routes::protagonist
