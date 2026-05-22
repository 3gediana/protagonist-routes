// A3.S1 - CommunicationMetrics unit tests.
//
// Covers per-channel emit count + unique-emitter / unique-listener
// dedup, listener_policy_shift L2 and cosine distance when active vs
// silent action distributions differ vs match, response_latency
// 5-second windowing, response_events change detection, and the
// appendCsv header/row format.

#include "runtime/CommunicationMetrics.h"

#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using neuro::AgentId;
using neuro::SimTimeSeconds;
using neuro::routes::protagonist::CommunicationMetrics;
using neuro::routes::protagonist::CommunicationChannelRow;

constexpr float kEps = 1.0e-4f;

AgentId makeAgentId(std::uint64_t v) { return AgentId{v}; }

std::vector<float> action(std::initializer_list<float> xs) {
    return std::vector<float>{xs};
}

}  // namespace

// recordEmit must count unique (emitter, tick) emits and dedup
// unique_emitters per channel.
TEST(CommunicationMetrics, RecordEmitCountsPerChannel) {
    CommunicationMetrics m(/*num_channels=*/4, /*action_dim=*/4);
    m.recordEmit(/*channel=*/0, makeAgentId(1), 1.0);
    m.recordEmit(/*channel=*/0, makeAgentId(2), 1.5);
    m.recordEmit(/*channel=*/0, makeAgentId(1), 2.0);
    m.recordEmit(/*channel=*/2, makeAgentId(3), 3.0);
    m.summarise(/*generation_index=*/0);
    const auto& rows = m.rowsPerChannel();
    ASSERT_EQ(rows.size(), 4u);
    EXPECT_EQ(rows[0].channel, 0u);
    EXPECT_EQ(rows[0].emit_count, 3u);
    EXPECT_EQ(rows[0].unique_emitter_count, 2u);
    EXPECT_EQ(rows[1].emit_count, 0u);
    EXPECT_EQ(rows[2].emit_count, 1u);
    EXPECT_EQ(rows[2].unique_emitter_count, 1u);
}

// listener_policy_shift_l2: when active and silent mean actions differ
// by [0.5, 0.0, 0.0, 0.0], L2 must be 0.5.
TEST(CommunicationMetrics, ListenerPolicyShiftL2) {
    CommunicationMetrics m(/*num_channels=*/1, /*action_dim=*/4);
    m.recordEmit(0, makeAgentId(1), 0.0);
    // Active listener-tick: action = [1.0, 0, 0, 0]
    m.recordListenerAction(makeAgentId(10), action({1.0f, 0.0f, 0.0f, 0.0f}),
                            /*mask=*/0x1u, 0.1);
    // Silent listener-tick: action = [0.5, 0, 0, 0]
    m.recordListenerAction(makeAgentId(10), action({0.5f, 0.0f, 0.0f, 0.0f}),
                            /*mask=*/0x0u, 100.0);
    m.summarise(0);
    const auto& rows = m.rowsPerChannel();
    EXPECT_NEAR(rows[0].listener_policy_shift_l2, 0.5f, kEps);
    EXPECT_EQ(rows[0].listener_ticks, 1u);
    EXPECT_EQ(rows[0].silent_ticks, 1u);
    EXPECT_EQ(rows[0].unique_listener_count, 1u);
}

// listener_policy_shift_l2 must be 0 when active and silent mean
// actions are identical.
TEST(CommunicationMetrics, ListenerPolicyShiftL2IsZeroWhenIdentical) {
    CommunicationMetrics m(1, 4);
    m.recordEmit(0, makeAgentId(1), 0.0);
    m.recordListenerAction(makeAgentId(10), action({0.5f, 0.5f, 0.5f, 0.5f}),
                            /*mask=*/0x1u, 0.1);
    m.recordListenerAction(makeAgentId(10), action({0.5f, 0.5f, 0.5f, 0.5f}),
                            /*mask=*/0x0u, 100.0);
    m.summarise(0);
    EXPECT_NEAR(m.rowsPerChannel()[0].listener_policy_shift_l2, 0.0f, kEps);
    EXPECT_NEAR(m.rowsPerChannel()[0].listener_policy_shift_cos, 0.0f, kEps);
}

// Cosine distance must be near 0 when action vectors point the same
// direction (regardless of magnitude). It must approach 1 when they
// are orthogonal.
TEST(CommunicationMetrics, CosineDistanceOrthogonalActions) {
    CommunicationMetrics m(1, 2);
    m.recordEmit(0, makeAgentId(1), 0.0);
    // Active mean = [1, 0]
    m.recordListenerAction(makeAgentId(10), action({1.0f, 0.0f}), 0x1u, 0.1);
    // Silent mean = [0, 1]
    m.recordListenerAction(makeAgentId(10), action({0.0f, 1.0f}), 0x0u, 100.0);
    m.summarise(0);
    EXPECT_NEAR(m.rowsPerChannel()[0].listener_policy_shift_cos, 1.0f, kEps);
}

// response_latency: only listener-ticks within 5 seconds of an emit
// count. A listener tick 20 seconds after the last emit must NOT
// inflate the latency average.
TEST(CommunicationMetrics, ResponseLatencyOnlyWithinFiveSeconds) {
    CommunicationMetrics m(1, 2);
    m.recordEmit(0, makeAgentId(1), /*emit_time=*/10.0);
    // Listener tick at t=11 (delta = 1.0s, in window)
    m.recordListenerAction(makeAgentId(10), action({0.0f, 0.0f}), 0x1u, 11.0);
    // Listener tick at t=13 (delta = 3.0s, in window)
    m.recordListenerAction(makeAgentId(10), action({0.0f, 0.0f}), 0x1u, 13.0);
    // Listener tick at t=30 (delta = 20s, OUT of window) -> ignored
    m.recordListenerAction(makeAgentId(10), action({0.0f, 0.0f}), 0x1u, 30.0);
    m.summarise(0);
    const auto& r = m.rowsPerChannel()[0];
    EXPECT_EQ(r.latency_sample_count, 2u);
    // Mean of (1.0, 3.0) = 2.0
    EXPECT_NEAR(r.response_latency_avg_seconds, 2.0f, kEps);
}

// When there are no emits, latency must be NaN; downstream code must
// be able to detect "no signal happened this generation".
TEST(CommunicationMetrics, LatencyIsNaNWithoutEmits) {
    CommunicationMetrics m(1, 2);
    m.recordListenerAction(makeAgentId(10), action({0.1f, 0.1f}), 0x0u, 5.0);
    m.summarise(0);
    EXPECT_TRUE(std::isnan(m.rowsPerChannel()[0].response_latency_avg_seconds));
}

// response_events: counts (listener_tick where active action differs
// from the same listener's most-recent silent action by > eps).
TEST(CommunicationMetrics, ResponseEventsDetectsActionShift) {
    CommunicationMetrics m(1, 2);
    m.recordEmit(0, makeAgentId(1), 0.0);
    // Silent first - establishes baseline action [0, 0]
    m.recordListenerAction(makeAgentId(10), action({0.0f, 0.0f}), 0x0u, 0.5);
    // Then active with a clearly different action [1, 0]
    m.recordListenerAction(makeAgentId(10), action({1.0f, 0.0f}), 0x1u, 1.0);
    // Active again with same action - should still count (vs silent baseline)
    m.recordListenerAction(makeAgentId(10), action({1.0f, 0.0f}), 0x1u, 2.0);
    // Active with action matching the silent baseline - should NOT count
    m.recordListenerAction(makeAgentId(10), action({0.0f, 0.0f}), 0x1u, 3.0);
    m.summarise(0);
    EXPECT_EQ(m.rowsPerChannel()[0].response_events, 2u);
}

// clear() must reset all state, so a single CommunicationMetrics
// instance can be reused across generations.
TEST(CommunicationMetrics, ClearResetsAllState) {
    CommunicationMetrics m(2, 2);
    m.recordEmit(0, makeAgentId(1), 1.0);
    m.recordListenerAction(makeAgentId(10), action({0.5f, 0.5f}), 0x1u, 1.5);
    m.summarise(0);
    EXPECT_GT(m.rowsPerChannel()[0].emit_count, 0u);
    m.clear();
    m.summarise(1);
    for (const auto& r : m.rowsPerChannel()) {
        EXPECT_EQ(r.emit_count, 0u);
        EXPECT_EQ(r.listener_ticks, 0u);
        EXPECT_EQ(r.silent_ticks, 0u);
    }
}

// appendCsv: header on first write, rows append on second write.
TEST(CommunicationMetrics, AppendCsvWritesHeaderAndRows) {
    const auto path = std::filesystem::temp_directory_path() / "neural_eco_test_comm_metrics.csv";
    std::error_code ec;
    std::filesystem::remove(path, ec);

    CommunicationMetrics m(2, 2);
    m.recordEmit(0, makeAgentId(1), 1.0);
    m.recordListenerAction(makeAgentId(10), action({0.5f, 0.5f}), 0x1u, 1.5);
    m.recordListenerAction(makeAgentId(10), action({0.0f, 0.0f}), 0x0u, 100.0);
    m.summarise(7);
    m.appendCsv(path, "test_run");

    std::ifstream in(path);
    ASSERT_TRUE(in.is_open());
    std::string line1;
    ASSERT_TRUE(std::getline(in, line1));
    EXPECT_NE(line1.find("scenario_name"), std::string::npos);
    EXPECT_NE(line1.find("listener_policy_shift_l2"), std::string::npos);

    std::string row;
    bool found_test_run = false;
    while (std::getline(in, row)) {
        if (row.find("test_run") != std::string::npos) {
            found_test_run = true;
            break;
        }
    }
    EXPECT_TRUE(found_test_run);
    in.close();
    std::filesystem::remove(path, ec);
}

// Multi-channel: emit on channel 1 must not pollute channel 0's
// active stats; listener-tick with mask 0x2 must update channel 1
// active and channels 0, 2, 3 silent.
TEST(CommunicationMetrics, ChannelIsolation) {
    CommunicationMetrics m(4, 2);
    m.recordEmit(1, makeAgentId(1), 0.0);
    // Listener heard channel 1 only.
    m.recordListenerAction(makeAgentId(10), action({1.0f, 0.0f}), 0x2u, 0.5);
    m.summarise(0);
    const auto& rows = m.rowsPerChannel();
    EXPECT_EQ(rows[0].listener_ticks, 0u);  // channel 0: silent for this listener
    EXPECT_EQ(rows[0].silent_ticks, 1u);
    EXPECT_EQ(rows[1].listener_ticks, 1u);  // channel 1: active
    EXPECT_EQ(rows[1].silent_ticks, 0u);
    EXPECT_EQ(rows[2].silent_ticks, 1u);
    EXPECT_EQ(rows[3].silent_ticks, 1u);
}
