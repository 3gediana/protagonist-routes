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
    // 2026-05-25 disk-budget cleanup: defaults previously wrote agent
    // pose every 10 substeps and world tile snapshots every 10 ticks,
    // yielding ~70-150 MB per traced episode. New defaults are 3x/5x
    // sparser respectively; toml can still override per-run. Long
    // 60-gen runs that opt into full tracing (e.g. via max_generations
    // = 60) will now write ~1-2 GB instead of ~7-8 GB.
    bool agents_enabled{true};
    bool world_enabled{true};
    std::size_t agent_interval{30};
    std::size_t world_interval{50};
    std::size_t max_generations{1};
    std::size_t max_episodes_per_generation{1};
    // L7 R1 (FINAL_BLUEPRINT.md §7): frame-based JSON dumper for Godot/raylib
    // viewer consumption. Each tick writes one JSON object to frames.jsonl
    // with agents/worksites/weather/season snapshots. Off by default (large).
    bool viewer_frames_enabled{false};
    std::size_t viewer_frame_interval{80};  // every N ticks (default sparse for disk budget)
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
    // L7 R1: viewer-friendly frame JSON
    void writeViewerFrame(World2D& world);

    World2D& world_;
    TraceRecorderConfig config_;
    std::filesystem::path trace_dir_;
    std::ofstream event_trace_;
    std::ofstream agent_trace_;
    std::ofstream world_trace_;
    std::ofstream viewer_frames_;  // L7 R1
    events::SubscriptionId event_subscription_{0};
};

TraceRecorderConfig loadTraceRecorderConfig(const config::Config& config);
bool shouldTraceEpisode(const TraceRecorderConfig& config, std::size_t generation, std::size_t episode);

}  // namespace neuro::routes::protagonist
