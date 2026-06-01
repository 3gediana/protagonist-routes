#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/types/Aliases.h"

namespace neuro::routes::protagonist {

// A3.S1 CommunicationMetrics: per-evaluatePopulation collector for signal
// emit / listener observation data. Replaces three scattered counters
// (signal_producer_count / signal_listener_count / signal_responder_count)
// with a per-channel breakdown plus listener-policy-shift / response-latency
// aggregates that match WORLD_UNDERSTANDING § 7.4.
//
// Per-evaluatePopulation lifecycle: instantiate at top, call recordEmit /
// recordListenerAction as the world ticks, call summarise() at end, query
// rowsPerChannel() for the metrics.csv row, flush appendCsv(path, tag).
// The caller is responsible for deciding what "channel_heard_mask" means
// (i.e. which channels are active in the listener's receptive field for
// this tick) — this class is wire-protocol-agnostic and unit-testable
// without a World instance.
struct CommunicationChannelRow {
    std::size_t generation{0};
    std::size_t channel{0};
    // Counts.
    std::size_t emit_count{0};            // unique (emitter_id, tick) emits on this channel
    std::size_t unique_emitter_count{0};  // unique emitter agent ids
    std::size_t unique_listener_count{0}; // unique listener agent ids that heard this channel at least once
    std::size_t listener_ticks{0};        // tick-count of (listener, this channel active) samples
    std::size_t silent_ticks{0};          // tick-count of (listener, this channel silent) samples
    std::size_t response_events{0};       // listener_ticks where action vector differed from silent baseline
    // Listener policy shift: L2 distance between mean(action | this channel active)
    // and mean(action | this channel silent). Zero if either group is empty.
    float listener_policy_shift_l2{0.0f};
    // Cosine distance variant (0 = same direction, 2 = opposite). NaN-clamped.
    float listener_policy_shift_cos{0.0f};
    // Mean seconds from emit_time to next listener action change after the
    // emit. NaN means no measurable latency this generation.
    float response_latency_avg_seconds{0.0f};
    std::size_t latency_sample_count{0};
};

class CommunicationMetrics {
public:
    explicit CommunicationMetrics(std::size_t num_channels, std::size_t action_dim);

    // Record one emit event. emit_time_seconds is the world clock at emit;
    // emitter is the agent id (may be kInvalidAgentId for controlled emitter).
    void recordEmit(std::size_t channel,
                    AgentId emitter,
                    SimTimeSeconds emit_time_seconds);

    // Record one listener tick: a (listener_id, action_vec, channel_active_mask)
    // sample at time_seconds. mask bit c = 1 means listener heard a non-zero
    // signal on channel c during this tick. action_vec is whatever the
    // caller wants to compare distributions over (e.g. brain output before
    // capability dispatch). action_vec.size() must == action_dim from
    // ctor.
    void recordListenerAction(AgentId listener,
                              const std::vector<float>& action_vec,
                              std::uint32_t channel_active_mask,
                              SimTimeSeconds time_seconds);

    // Finalise statistics (mean shift, latency). Idempotent; safe to call
    // multiple times. Must be called before rowsPerChannel() returns
    // useful policy_shift / latency_avg values.
    void summarise(std::size_t generation_index);

    const std::vector<CommunicationChannelRow>& rowsPerChannel() const { return rows_; }

    // Append summarised rows to CSV at path. Writes header when file does
    // not exist or is empty. `tag` is written into the scenario_name
    // column for distinguishing ablation runs.
    void appendCsv(const std::filesystem::path& path, std::string_view tag) const;

    // Reset all state (for next generation).
    void clear();

    std::size_t numChannels() const { return num_channels_; }
    std::size_t actionDim() const { return action_dim_; }

private:
    struct ChannelAccumulator {
        std::size_t emit_count{0};
        std::unordered_set<std::int64_t> unique_emitters;
        std::unordered_set<std::int64_t> unique_listeners;
        std::vector<double> action_sum_active;   // size = action_dim
        std::vector<double> action_sum_silent;   // size = action_dim
        std::size_t active_ticks{0};
        std::size_t silent_ticks{0};
        // Last emit time per emitter on this channel (for latency tracking).
        std::vector<SimTimeSeconds> recent_emit_times;
        double latency_sum_seconds{0.0};
        std::size_t latency_sample_count{0};
        // Listener last-active-action snapshot, for response_events counter.
        std::unordered_map<std::int64_t, std::vector<float>> last_silent_action;
        std::size_t response_events{0};
    };

    std::size_t num_channels_{0};
    std::size_t action_dim_{0};
    std::vector<ChannelAccumulator> per_channel_;
    std::vector<CommunicationChannelRow> rows_;
    bool summarised_{false};
};

}  // namespace neuro::routes::protagonist
