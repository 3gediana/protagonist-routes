#pragma once

#include <cstdint>
#include <filesystem>
#include <set>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/types/Aliases.h"

namespace neuro::events {
struct BuildCompleted;
}

namespace neuro::routes::protagonist {

// A2.Step2 CreditLedger: centralises share-based credit settlement for
// build / hunt / signal cooperative rewards. Replaces three scattered
// share-computation blocks in ProtagonistEcologyRuntime::evaluatePopulation.
//
// Per-evaluatePopulation lifecycle: instantiate at top, call settleBuild /
// settleHunt / settleSignal as those events fire, query fairness at end,
// flush to _credit_log.csv. The caller is still responsible for:
//   - adding payout.reward to the recipient agent's fitness
//   - publishing the events::CreditAssigned event on the EventBus
// (so this class stays world-/event-bus-agnostic and is unit-testable
// without a World instance).
struct CreditPayout {
    Tick tick{0};
    AgentId recipient{};
    std::uint8_t source_kind{0};  // 0 = build, 1 = hunt, 2 = signal_response
    std::uint64_t source_id{0};
    float share{0.0f};
    float reward{0.0f};
};

// Aggregated fairness metrics across one or more source kinds.
//   freeloader_ratio: fraction of recorded contributors whose share is
//                     below kFreeloaderEpsilon (default 0.01 = 1%).
//                     Lower = fewer free riders.
//   share_gini:       Gini coefficient over the share distribution. 0 =
//                     perfectly even split, approaches 1 as one
//                     contributor takes everything.
//   top_share_mean:   mean of "top contributor's share" per source event.
//                     0 = no source had any clear leader; 1 = every
//                     source had a single contributor taking 100%.
//   total_reward:     sum of payout.reward for selected source kinds (in
//                     fitness units, already weighted).
//   payout_count:     number of CreditPayout rows considered.
//   source_count:     number of distinct (source_kind, source_id) events.
struct CreditFairnessMetrics {
    std::size_t payout_count{0};
    std::size_t source_count{0};
    float freeloader_ratio{0.0f};
    float share_gini{0.0f};
    float top_share_mean{0.0f};
    float total_reward{0.0f};
};

class CreditLedger {
public:
    static constexpr float kFreeloaderEpsilon = 0.01f;

    CreditLedger() = default;

    // Compute build credit payouts using time-share among contributors
    // whose contribution_seconds >= min_seconds. reward_scale is the
    // total reward to distribute (already multiplied by whatever weight
    // the caller wants, e.g. cooperation_weight * worksite_completion_reward).
    // Each returned CreditPayout.reward = share * reward_scale.
    // Appends the same payouts to the internal log.
    std::vector<CreditPayout> settleBuild(Tick tick,
                                          const events::BuildCompleted& build,
                                          float min_seconds,
                                          float reward_scale);

    // Compute hunt credit payouts using damage-share among attackers
    // whose share >= min_share (after normalising). Same reward_scale
    // semantics as settleBuild.
    std::vector<CreditPayout> settleHunt(Tick tick,
                                         AgentId victim,
                                         const std::unordered_map<AgentId, float>& attacker_damage,
                                         float min_share,
                                         float reward_scale);

    // Compute signal credit payouts using dwell-time-share among
    // listeners. No minimum-share filter (any non-zero dwell counts).
    // emitter is recorded for trace lineage; can be kInvalidAgentId for
    // the controlled emitter.
    std::vector<CreditPayout> settleSignal(Tick emit_tick,
                                           AgentId emitter,
                                           const std::unordered_map<AgentId, float>& listener_dwell,
                                           float reward_scale);

    // Compute fairness over a subset of logged payouts. source_kind_filter
    // = 0xFF includes all. Returns zeros if there are no matching payouts.
    CreditFairnessMetrics computeFairness(std::uint8_t source_kind_filter = 0xFF) const;

    // Append all logged payouts to a CSV file at path. Writes a header
    // row when the file does not exist or is empty. `tag` is written into
    // every row's `scenario_name` column (so multiple ablation runs into
    // the same file are distinguishable).
    void appendCsv(const std::filesystem::path& path, std::string_view tag) const;

    // Reset internal log (e.g. between episodes).
    void clear();

    const std::vector<CreditPayout>& payouts() const { return payouts_; }

private:
    // Dedup log: a single source event (e.g. one BuildCompleted) may be
    // processed by each protagonist agent in the outer loop, but we only
    // want to log its payouts once. Each settleXxx() consults this set
    // before appending to payouts_; the returned payouts vector is still
    // complete on every call so callers can pick out their own share.
    std::vector<CreditPayout> payouts_;
    std::set<std::pair<std::uint8_t, std::uint64_t>> settled_sources_;
};

}  // namespace neuro::routes::protagonist
