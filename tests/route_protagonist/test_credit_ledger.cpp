// A2.Step3 - CreditLedger unit tests.
//
// Covers share computation correctness for the three settle paths
// (build / hunt / signal), dedup behaviour when the same source event
// is settled multiple times (mirrors the per-protagonist iteration
// pattern in evaluatePopulation), the freeloader / Gini / top_share
// fairness aggregation, and the appendCsv header/row format.

#include "core/events/EventPayloads.h"
#include "runtime/CreditLedger.h"

#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

using neuro::AgentId;
using neuro::Vec2;
using neuro::events::BuildCompleted;
using neuro::routes::protagonist::CreditLedger;
using neuro::routes::protagonist::CreditPayout;

constexpr float kEps = 1.0e-5f;

AgentId makeAgentId(std::uint64_t v) { return AgentId{v}; }

}  // namespace

// settleSignal: dwell time share must sum to 1.0 across all listeners,
// each individual share = listener_dwell / total_dwell.
TEST(CreditLedger, SettleSignalProducesDwellTimeShare) {
    CreditLedger ledger;
    std::unordered_map<AgentId, float> dwell;
    dwell[makeAgentId(1)] = 5.0f;
    dwell[makeAgentId(2)] = 3.0f;
    dwell[makeAgentId(3)] = 2.0f;

    const auto payouts = ledger.settleSignal(
        /*emit_tick=*/100,
        /*emitter=*/makeAgentId(0),
        dwell,
        /*reward_scale=*/10.0f);

    ASSERT_EQ(payouts.size(), 3u);
    float total_share = 0.0f;
    float total_reward = 0.0f;
    for (const auto& p : payouts) {
        EXPECT_EQ(p.source_kind, 2u);
        EXPECT_EQ(p.source_id, 100u);
        total_share += p.share;
        total_reward += p.reward;
    }
    EXPECT_NEAR(total_share, 1.0f, kEps);
    EXPECT_NEAR(total_reward, 10.0f, kEps);

    // Individual shares
    for (const auto& p : payouts) {
        if (p.recipient.value == 1) EXPECT_NEAR(p.share, 0.5f, kEps);
        if (p.recipient.value == 2) EXPECT_NEAR(p.share, 0.3f, kEps);
        if (p.recipient.value == 3) EXPECT_NEAR(p.share, 0.2f, kEps);
    }
}

// settleSignal: empty dwell map produces no payouts and no log entries.
TEST(CreditLedger, SettleSignalEmptyDwellProducesNothing) {
    CreditLedger ledger;
    std::unordered_map<AgentId, float> dwell;

    const auto payouts = ledger.settleSignal(50, makeAgentId(0), dwell, 100.0f);

    EXPECT_TRUE(payouts.empty());
    EXPECT_TRUE(ledger.payouts().empty());
}

// settleHunt: damage share with min_share threshold, attackers below
// threshold are dropped.
TEST(CreditLedger, SettleHuntFiltersBelowMinShare) {
    CreditLedger ledger;
    std::unordered_map<AgentId, float> damage;
    damage[makeAgentId(10)] = 95.0f;  // 95% share
    damage[makeAgentId(11)] = 4.0f;   // 4% share
    damage[makeAgentId(12)] = 0.5f;   // 0.5% share -> filtered
    damage[makeAgentId(13)] = 0.5f;   // 0.5% share -> filtered

    const auto payouts = ledger.settleHunt(
        /*tick=*/500,
        /*victim=*/makeAgentId(99),
        damage,
        /*min_share=*/0.01f,
        /*reward_scale=*/100.0f);

    // Two attackers above the threshold.
    ASSERT_EQ(payouts.size(), 2u);
    float total_share = 0.0f;
    for (const auto& p : payouts) {
        EXPECT_EQ(p.source_kind, 1u);
        EXPECT_EQ(p.source_id, 99u);
        EXPECT_NE(p.recipient.value, 12u);
        EXPECT_NE(p.recipient.value, 13u);
        total_share += p.share;
    }
    // Filtered shares are still counted in the denominator (total damage
    // includes them), so the surviving shares are 95/100 + 4/100 = 0.99,
    // NOT 95/99 + 4/99. This is the same behaviour as the pre-refactor
    // runtime code.
    EXPECT_NEAR(total_share, 0.99f, kEps);
}

// settleBuild: time-share with min_seconds threshold.
TEST(CreditLedger, SettleBuildProducesTimeShare) {
    CreditLedger ledger;
    BuildCompleted build;
    build.worksite_id = 7;
    build.site_pos = Vec2{50.0f, 50.0f};
    build.contributors = {makeAgentId(1), makeAgentId(2), makeAgentId(3), makeAgentId(4)};
    build.contribution_seconds = {6.0f, 3.0f, 1.0f, 0.1f};  // last one below 0.5s
    build.duration_seconds = 10.0;
    build.started_at_seconds = 0.0;

    const auto payouts = ledger.settleBuild(
        /*tick=*/200,
        build,
        /*min_seconds=*/0.5f,
        /*reward_scale=*/200.0f);

    // 3 contributors above threshold, 4th dropped.
    ASSERT_EQ(payouts.size(), 3u);
    float total_share = 0.0f;
    for (const auto& p : payouts) {
        EXPECT_EQ(p.source_kind, 0u);
        EXPECT_EQ(p.source_id, 7u);
        EXPECT_NE(p.recipient.value, 4u);
        total_share += p.share;
    }
    // Top contributor takes 6/10 = 0.6, next 3/10 = 0.3, last 1/10 = 0.1.
    EXPECT_NEAR(total_share, 1.0f, kEps);
}

// Dedup: calling settleBuild twice with the same worksite_id only logs
// the payouts once (the second call returns the same vector but doesn't
// duplicate into the internal log).
TEST(CreditLedger, SettleBuildDedupsRepeatLogEntries) {
    CreditLedger ledger;
    BuildCompleted build;
    build.worksite_id = 5;
    build.site_pos = Vec2{0.0f, 0.0f};
    build.contributors = {makeAgentId(1), makeAgentId(2)};
    build.contribution_seconds = {2.0f, 1.0f};
    build.duration_seconds = 3.0;
    build.started_at_seconds = 0.0;

    const auto first = ledger.settleBuild(100, build, 0.5f, 60.0f);
    const auto second = ledger.settleBuild(100, build, 0.5f, 60.0f);
    const auto third = ledger.settleBuild(100, build, 0.5f, 60.0f);

    // Every call returns the full payout list.
    EXPECT_EQ(first.size(), 2u);
    EXPECT_EQ(second.size(), 2u);
    EXPECT_EQ(third.size(), 2u);
    // But the internal log only carries the payouts from the first call.
    EXPECT_EQ(ledger.payouts().size(), 2u);
}

// Dedup is per (source_kind, source_id): the same source_id reused
// across different kinds does NOT collide.
TEST(CreditLedger, DedupKeyIsKindIdPair) {
    CreditLedger ledger;
    BuildCompleted build;
    build.worksite_id = 42;
    build.contributors = {makeAgentId(1)};
    build.contribution_seconds = {5.0f};
    build.duration_seconds = 5.0;
    build.started_at_seconds = 0.0;

    std::unordered_map<AgentId, float> damage;
    damage[makeAgentId(2)] = 100.0f;

    std::unordered_map<AgentId, float> dwell;
    dwell[makeAgentId(3)] = 1.0f;

    // source_id 42 used by all three kinds.
    ledger.settleBuild(/*tick=*/10, build, 0.5f, 100.0f);
    ledger.settleHunt(/*tick=*/20, AgentId{42}, damage, 0.01f, 50.0f);
    ledger.settleSignal(/*emit_tick=*/42, AgentId{0}, dwell, 30.0f);

    // 1 + 1 + 1 payouts logged, no collision.
    EXPECT_EQ(ledger.payouts().size(), 3u);
}

// computeFairness: a perfectly even split should give Gini = 0,
// freeloader_ratio = 0 (no one below 1%), top_share_mean = 1/N.
TEST(CreditLedger, FairnessEvenSplit) {
    CreditLedger ledger;
    std::unordered_map<AgentId, float> dwell;
    for (std::uint64_t i = 1; i <= 4; ++i) {
        dwell[AgentId{i}] = 5.0f;  // equal dwell
    }
    ledger.settleSignal(0, AgentId{0}, dwell, 100.0f);

    const auto m = ledger.computeFairness();
    EXPECT_EQ(m.payout_count, 4u);
    EXPECT_EQ(m.source_count, 1u);
    EXPECT_NEAR(m.share_gini, 0.0f, kEps);
    EXPECT_NEAR(m.freeloader_ratio, 0.0f, kEps);
    EXPECT_NEAR(m.top_share_mean, 0.25f, kEps);
    EXPECT_NEAR(m.total_reward, 100.0f, kEps);
}

// computeFairness: one dominant contributor + many freeloaders.
// freeloader_ratio = (N-1)/N, top_share_mean approaches 1.
TEST(CreditLedger, FairnessDominantContributor) {
    CreditLedger ledger;
    std::unordered_map<AgentId, float> dwell;
    dwell[AgentId{1}] = 99.0f;
    for (std::uint64_t i = 2; i <= 10; ++i) {
        dwell[AgentId{i}] = 0.1f;  // each ~0.1% share, below 1% threshold
    }
    ledger.settleSignal(0, AgentId{0}, dwell, 100.0f);

    const auto m = ledger.computeFairness();
    EXPECT_EQ(m.payout_count, 10u);
    EXPECT_EQ(m.source_count, 1u);
    // 9 listeners with share < 1% out of 10 = 0.9.
    EXPECT_NEAR(m.freeloader_ratio, 0.9f, kEps);
    // Gini close to 0.9.
    EXPECT_GT(m.share_gini, 0.8f);
    EXPECT_LT(m.share_gini, 1.0f);
    // Top contributor takes 99/(99 + 9 * 0.1) = 99/99.9 ~ 0.991.
    EXPECT_GT(m.top_share_mean, 0.98f);
}

// computeFairness with source_kind_filter selects only matching payouts.
TEST(CreditLedger, FairnessFilterByKind) {
    CreditLedger ledger;
    BuildCompleted build;
    build.worksite_id = 1;
    build.contributors = {AgentId{1}, AgentId{2}};
    build.contribution_seconds = {2.0f, 2.0f};
    build.duration_seconds = 4.0;
    build.started_at_seconds = 0.0;
    ledger.settleBuild(10, build, 0.5f, 100.0f);

    std::unordered_map<AgentId, float> dwell;
    dwell[AgentId{3}] = 10.0f;  // alone, share = 1.0
    ledger.settleSignal(20, AgentId{0}, dwell, 50.0f);

    // Filter for build only: 2 payouts, Gini = 0 (equal split).
    const auto m_build = ledger.computeFairness(/*source_kind_filter=*/0u);
    EXPECT_EQ(m_build.payout_count, 2u);
    EXPECT_EQ(m_build.source_count, 1u);
    EXPECT_NEAR(m_build.share_gini, 0.0f, kEps);

    // Filter for signal only: 1 payout, top_share_mean = 1.0.
    const auto m_sig = ledger.computeFairness(/*source_kind_filter=*/2u);
    EXPECT_EQ(m_sig.payout_count, 1u);
    EXPECT_NEAR(m_sig.top_share_mean, 1.0f, kEps);

    // No filter: union of both.
    const auto m_all = ledger.computeFairness();
    EXPECT_EQ(m_all.payout_count, 3u);
}

// appendCsv: header on first write, content rows include tag.
TEST(CreditLedger, AppendCsvWritesHeaderAndRows) {
    const auto tmp_dir = std::filesystem::temp_directory_path() / "credit_ledger_test";
    std::filesystem::create_directories(tmp_dir);
    const auto csv_path = tmp_dir / "credit_log.csv";
    std::filesystem::remove(csv_path);

    CreditLedger ledger;
    std::unordered_map<AgentId, float> dwell;
    dwell[AgentId{7}] = 10.0f;
    ledger.settleSignal(/*emit_tick=*/300, AgentId{0}, dwell, /*reward_scale=*/5.0f);

    ledger.appendCsv(csv_path, "test_scenario/ep_42");

    {
        std::ifstream in(csv_path);
        ASSERT_TRUE(in.good());
        std::string header;
        std::getline(in, header);
        EXPECT_EQ(header, "scenario_name,tick,source_kind,source_id,recipient_id,share,reward");

        std::string row;
        std::getline(in, row);
        // Expect: test_scenario/ep_42,300,2,300,7,1,5
        EXPECT_NE(row.find("test_scenario/ep_42"), std::string::npos);
        EXPECT_NE(row.find(",2,300,7,"), std::string::npos);
    }  // close ifstream before removing (Windows locks open files)

    std::error_code ec;
    std::filesystem::remove(csv_path, ec);
    std::filesystem::remove(tmp_dir, ec);
}
