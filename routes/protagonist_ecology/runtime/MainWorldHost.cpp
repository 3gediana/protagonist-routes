#include "runtime/MainWorldHost.h"

#include "runtime/BootstrapPopulation.h"
#include "runtime/ProtagonistEcologyConfig.h"
#include "runtime/ProtagonistWorldFactory.h"

#include "core/agent/Agent.h"
#include "core/world/World2D.h"
#include "core/world/layers/AgentLayer.h"
#include "world/BaseLayer.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

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

std::size_t MainWorldHost::saveAgentsSnapshot(const std::filesystem::path& dir) const {
    if (world_ == nullptr) {
        spdlog::warn("MainWorldHost::saveAgentsSnapshot: world_ is null; skipping");
        return 0;
    }
    const auto* agent_layer = world_->getLayer<AgentLayer>();
    if (agent_layer == nullptr) {
        spdlog::warn("MainWorldHost::saveAgentsSnapshot: world has no AgentLayer; skipping");
        return 0;
    }

    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        spdlog::error("MainWorldHost::saveAgentsSnapshot: create_directories({}) failed: {}",
                      dir.string(), ec.message());
        return 0;
    }

    std::ostringstream entries;
    bool first = true;
    std::size_t saved = 0;
    for (const Agent* agent : agent_layer->agents()) {
        if (agent == nullptr) continue;
        if (!agent->alive()) continue;
        if (!first) entries << ",\n";
        first = false;
        const Vec2 pos = agent->position();
        entries << "    {"
                << "\"agent_id\":" << agent->id().value
                << ",\"archetype_tag\":" << static_cast<unsigned>(agent->archetypeTag())
                << ",\"species_id\":" << agent->speciesId().value
                << ",\"genome_id\":" << agent->genomeId().value
                << ",\"lineage_id\":" << agent->lineageId()
                << ",\"position\":[" << pos.x << "," << pos.y << "]"
                << "}";
        ++saved;
    }

    const auto manifest_path = dir / "main_world_agents.json";
    std::ofstream out(manifest_path);
    if (!out) {
        spdlog::error("MainWorldHost::saveAgentsSnapshot: failed to open {} for write",
                      manifest_path.string());
        return 0;
    }
    out << "{\n"
        << "  \"version\": 1,\n"
        << "  \"snapshot_sim_time\": " << simTimeSeconds() << ",\n"
        << "  \"tick_call_count\": " << tickCallCount() << ",\n"
        << "  \"cumulative_seed_count\": " << seedCount() << ",\n"
        << "  \"agent_count\": " << saved << ",\n"
        << "  \"agents\": [";
    if (saved > 0) {
        out << "\n" << entries.str() << "\n  ";
    }
    out << "]\n"
        << "}\n";
    return saved;
}

}  // namespace neuro::routes::protagonist
