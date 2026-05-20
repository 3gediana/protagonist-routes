#pragma once

#include "core/events/Event.h"
#include "core/interfaces/IEventBus.h"
#include "runtime/ProtagonistEcologyConfig.h"

#include <cstddef>
#include <filesystem>
#include <fstream>

namespace neuro {
class World2D;
}

namespace neuro::routes::protagonist {

struct TraceRecorderConfig {
    bool enabled{false};
    bool events_enabled{true};
    bool agents_enabled{true};
    bool world_enabled{true};
    std::size_t agent_interval{10};
    std::size_t world_interval{10};
    std::size_t max_generations{1};
    std::size_t max_episodes_per_generation{1};
};

class TraceRecorder final {
public:
    TraceRecorder(World2D& world,
                  std::filesystem::path trace_dir,
                  const ProtagonistEcologyConfig& route,
                  const TraceRecorderConfig& config,
                  std::size_t generation,
                  std::size_t episode);
    ~TraceRecorder();

    TraceRecorder(const TraceRecorder&) = delete;
    TraceRecorder& operator=(const TraceRecorder&) = delete;

    bool active() const;
    void recordTick(World2D& world);

private:
    void writeManifest(const ProtagonistEcologyConfig& route, std::size_t generation, std::size_t episode);
    void writeEvent(const events::Event& event);
    void writeAgentSnapshot(World2D& world);
    void writeWorldSnapshot(World2D& world);

    World2D& world_;
    TraceRecorderConfig config_;
    std::filesystem::path trace_dir_;
    std::ofstream event_trace_;
    std::ofstream agent_trace_;
    std::ofstream world_trace_;
    events::SubscriptionId event_subscription_{0};
};

TraceRecorderConfig loadTraceRecorderConfig(const config::Config& config);
bool shouldTraceEpisode(const TraceRecorderConfig& config, std::size_t generation, std::size_t episode);

}  // namespace neuro::routes::protagonist
