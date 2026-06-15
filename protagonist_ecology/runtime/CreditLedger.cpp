#include "runtime/CreditLedger.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <set>
#include <utility>

#include "core/logging/Logger.h"

#include "core/events/EventPayloads.h"

namespace neuro::routes::protagonist {

std::vector<CreditPayout> CreditLedger::settleBuild(Tick tick,
                                                    const events::BuildCompleted& build,
                                                    float min_seconds,
                                                    float reward_scale) {
    std::vector<CreditPayout> out;
    if (build.contributors.size() != build.contribution_seconds.size()) {
        return out;
    }
    float total_secs = 0.0f;
    for (float s : build.contribution_seconds) {
        if (s >= min_seconds) total_secs += s;
    }
    if (total_secs <= 0.0f) return out;

    const std::pair<std::uint8_t, std::uint64_t> dedup_key{
        0u, static_cast<std::uint64_t>(build.worksite_id)};
    const bool first_settle = settled_sources_.insert(dedup_key).second;

    out.reserve(build.contributors.size());
    for (std::size_t i = 0; i < build.contributors.size(); ++i) {
        const float secs = build.contribution_seconds[i];
        if (secs < min_seconds) continue;
        const float share = secs / total_secs;
        const float reward = share * reward_scale;
        CreditPayout p;
        p.tick = tick;
        p.recipient = build.contributors[i];
        p.source_kind = 0u;
        p.source_id = static_cast<std::uint64_t>(build.worksite_id);
        p.share = share;
        p.reward = reward;
        out.push_back(p);
        if (first_settle) payouts_.push_back(p);
    }
    return out;
}

std::vector<CreditPayout> CreditLedger::settleHunt(Tick tick,
                                                   AgentId victim,
                                                   const std::unordered_map<AgentId, float>& attacker_damage,
                                                   float min_share,
                                                   float reward_scale) {
    std::vector<CreditPayout> out;
    float total_damage = 0.0f;
    for (const auto& [_, dmg] : attacker_damage) total_damage += dmg;
    if (total_damage <= 0.0f) return out;

    const std::pair<std::uint8_t, std::uint64_t> dedup_key{
        1u, static_cast<std::uint64_t>(victim.value)};
    const bool first_settle = settled_sources_.insert(dedup_key).second;

    out.reserve(attacker_damage.size());
    for (const auto& [attacker_id, dmg] : attacker_damage) {
        const float share = dmg / total_damage;
        if (share < min_share) continue;
        const float reward = share * reward_scale;
        CreditPayout p;
        p.tick = tick;
        p.recipient = attacker_id;
        p.source_kind = 1u;
        p.source_id = static_cast<std::uint64_t>(victim.value);
        p.share = share;
        p.reward = reward;
        out.push_back(p);
        if (first_settle) payouts_.push_back(p);
    }
    return out;
}

std::vector<CreditPayout> CreditLedger::settleSignal(Tick emit_tick,
                                                     AgentId /*emitter*/,
                                                     const std::unordered_map<AgentId, float>& listener_dwell,
                                                     float reward_scale) {
    std::vector<CreditPayout> out;
    float total_secs = 0.0f;
    for (const auto& [_, secs] : listener_dwell) total_secs += secs;
    if (total_secs <= 1.0e-6f) return out;

    const std::pair<std::uint8_t, std::uint64_t> dedup_key{
        2u, static_cast<std::uint64_t>(emit_tick)};
    const bool first_settle = settled_sources_.insert(dedup_key).second;

    out.reserve(listener_dwell.size());
    for (const auto& [listener_id, secs] : listener_dwell) {
        const float share = secs / total_secs;
        const float reward = share * reward_scale;
        CreditPayout p;
        p.tick = emit_tick;
        p.recipient = listener_id;
        p.source_kind = 2u;
        p.source_id = static_cast<std::uint64_t>(emit_tick);
        p.share = share;
        p.reward = reward;
        out.push_back(p);
        if (first_settle) payouts_.push_back(p);
    }
    return out;
}

CreditFairnessMetrics CreditLedger::computeFairness(std::uint8_t source_kind_filter) const {
    CreditFairnessMetrics m;
    std::vector<float> shares;
    shares.reserve(payouts_.size());
    std::set<std::pair<std::uint8_t, std::uint64_t>> source_keys;
    std::unordered_map<std::uint64_t, float> top_share_per_source;
    std::size_t freeloader_count = 0;
    for (const auto& p : payouts_) {
        if (source_kind_filter != 0xFFu && p.source_kind != source_kind_filter) continue;
        shares.push_back(p.share);
        source_keys.insert({p.source_kind, p.source_id});
        m.total_reward += p.reward;
        if (p.share < kFreeloaderEpsilon) ++freeloader_count;
        // Pack (source_kind, source_id) into a single key for top-share tracking.
        const std::uint64_t packed = (static_cast<std::uint64_t>(p.source_kind) << 56)
                                   | (p.source_id & 0x00FFFFFFFFFFFFFFull);
        auto& top = top_share_per_source[packed];
        if (p.share > top) top = p.share;
    }
    m.payout_count = shares.size();
    m.source_count = source_keys.size();
    if (shares.empty()) return m;

    m.freeloader_ratio = static_cast<float>(freeloader_count) / static_cast<float>(shares.size());

    // Gini coefficient on the share distribution. With n payouts sorted
    // ascending, Gini = (2 * sum(i * s(i)) / (n * sum(s))) - (n + 1) / n.
    std::sort(shares.begin(), shares.end());
    const float n = static_cast<float>(shares.size());
    float sum_s = 0.0f;
    float weighted = 0.0f;
    for (std::size_t i = 0; i < shares.size(); ++i) {
        sum_s += shares[i];
        weighted += static_cast<float>(i + 1) * shares[i];
    }
    if (sum_s > 1.0e-9f) {
        m.share_gini = (2.0f * weighted) / (n * sum_s) - (n + 1.0f) / n;
        if (m.share_gini < 0.0f) m.share_gini = 0.0f;
    }

    // Mean of per-source top-1 share.
    float top_sum = 0.0f;
    for (const auto& [_, top] : top_share_per_source) top_sum += top;
    if (!top_share_per_source.empty()) {
        m.top_share_mean = top_sum / static_cast<float>(top_share_per_source.size());
    }
    return m;
}

void CreditLedger::appendCsv(const std::filesystem::path& path, std::string_view tag) const {
    // D-044 disk-budget guard. The 2026-05-23 incident grew _credit_log.csv
    // to 7.07 GB / 110M rows; later runs hit 2.1 GB before disk-budget
    // cleanup landed. Even with the per-run-dir routing in place, we keep a
    // hard 200 MB ceiling per file as a last line of defence so a single
    // ablation never silently grinds the disk to dust.
    constexpr std::uintmax_t kCapBytes = 200ull * 1024ull * 1024ull;
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    const auto current_size = std::filesystem::file_size(path, ec);
    if (!ec && current_size >= kCapBytes) {
        static std::set<std::filesystem::path> warned;
        if (warned.insert(path).second) {
            LOG_WARN("CreditLedger::appendCsv refusing to grow {} past {} MB cap",
                     path.string(), kCapBytes / (1024ull * 1024ull));
        }
        return;
    }
    const bool need_header = !std::filesystem::exists(path, ec)
                             || std::filesystem::file_size(path, ec) == 0;
    std::ofstream out(path, std::ios::app);
    if (!out.is_open()) return;
    if (need_header) {
        out << "scenario_name,tick,source_kind,source_id,recipient_id,share,reward\n";
    }
    for (const auto& p : payouts_) {
        out << tag << ','
            << p.tick << ','
            << static_cast<int>(p.source_kind) << ','
            << p.source_id << ','
            << p.recipient.value << ','
            << p.share << ','
            << p.reward << '\n';
    }
}

void CreditLedger::clear() {
    payouts_.clear();
    settled_sources_.clear();
}

}  // namespace neuro::routes::protagonist
