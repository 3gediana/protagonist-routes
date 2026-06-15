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
    // 2026-05-29 trajectory-analysis pivot: capture the LAST N
    // generations instead of (or in addition to) the FIRST N. When
    // > 0, an episode is traced if its generation index is >=
    // (total_generations - tail_generations). Useful for "I want
    // to see what the trained brain actually does at gen 79" while
    // staying within disk budget. Pass total_generations to
    // `shouldTraceEpisode` to use this; 0 disables the tail-mode.
    std::size_t tail_generations{0};
    // L7 R1 (FINAL_BLUEPRINT.md §7): frame-based JSON dumper for Godot/raylib
    // viewer consumption. Each tick writes one JSON object to frames.jsonl
    // with agents/worksites/weather/season snapshots. Off by default (large).
    bool viewer_frames_enabled{false};
    std::size_t viewer_frame_interval{80};  // every N ticks (default sparse for disk budget)

    // 2026-05-28 disk-budget guard (parity with D-044 CreditLedger /
    // CommunicationMetrics / _signal_debug.csv 200 MB caps). Per-file
    // ceiling on event_trace.jsonl / agent_trace.csv / world_trace.csv /
    // viewer_frames.jsonl. Once a stream's tellp() crosses the cap, the
    // recorder closes the stream and emits a single LOG_WARN. Safety net
    // against runs that opt into `max_generations = 60` and unintentionally
    // produce ~90 GB of traces (previously observed: each event_trace.jsonl
    // ~750 MB, 60 gen x 2 ep = ~90 GB per run). 0 means no cap.
    std::size_t max_bytes_per_file{200ULL * 1024ULL * 1024ULL};
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
    // L7 R4 (3D web viewer): one-time static world header (terrain
    // heightmap + water sources + resource cells) written alongside
    // viewer_frames.jsonl so the renderer can build the 3D map once.
    void writeViewerWorld(World2D& world, const ProtagonistEcologyConfig& route);

    World2D& world_;
    TraceRecorderConfig config_;
    std::filesystem::path trace_dir_;
    std::ofstream event_trace_;
    std::ofstream agent_trace_;
    std::ofstream world_trace_;
    std::ofstream viewer_frames_;  // L7 R1
    events::SubscriptionId event_subscription_{0};

    // 2026-05-28 disk-budget: tracks whether the per-file cap warning
    // has already been emitted for each stream so we don't spam the log.
    bool event_cap_warned_{false};
    bool agent_cap_warned_{false};
    bool world_cap_warned_{false};
    bool viewer_cap_warned_{false};
};

TraceRecorderConfig loadTraceRecorderConfig(const config::Config& config);
// Decides whether to attach a TraceRecorder to (generation, episode).
// total_generations is used only when tail_generations > 0 (overload below).
bool shouldTraceEpisode(const TraceRecorderConfig& config, std::size_t generation, std::size_t episode);
// Tail-mode aware overload: also traces when
// `generation >= total_generations - tail_generations`. The first overload
// remains for callers that don't know the total (pre-pivot call sites).
bool shouldTraceEpisode(const TraceRecorderConfig& config,
                        std::size_t generation,
                        std::size_t episode,
                        std::size_t total_generations);

}  // namespace neuro::routes::protagonist
