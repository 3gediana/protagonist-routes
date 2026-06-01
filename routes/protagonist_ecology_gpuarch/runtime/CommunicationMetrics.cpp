#include "runtime/CommunicationMetrics.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <set>

#include "core/logging/Logger.h"

namespace neuro::routes::protagonist {

namespace {

float l2(const std::vector<double>& a_sum, std::size_t a_n,
         const std::vector<double>& b_sum, std::size_t b_n) {
    if (a_n == 0 || b_n == 0) return 0.0f;
    double s = 0.0;
    for (std::size_t i = 0; i < a_sum.size() && i < b_sum.size(); ++i) {
        const double a_mean = a_sum[i] / static_cast<double>(a_n);
        const double b_mean = b_sum[i] / static_cast<double>(b_n);
        const double d = a_mean - b_mean;
        s += d * d;
    }
    return static_cast<float>(std::sqrt(s));
}

float cosineDistance(const std::vector<double>& a_sum, std::size_t a_n,
                     const std::vector<double>& b_sum, std::size_t b_n) {
    if (a_n == 0 || b_n == 0) return 0.0f;
    double dot = 0.0;
    double na = 0.0;
    double nb = 0.0;
    for (std::size_t i = 0; i < a_sum.size() && i < b_sum.size(); ++i) {
        const double am = a_sum[i] / static_cast<double>(a_n);
        const double bm = b_sum[i] / static_cast<double>(b_n);
        dot += am * bm;
        na += am * am;
        nb += bm * bm;
    }
    const double denom = std::sqrt(na) * std::sqrt(nb);
    if (denom < 1.0e-9) return 0.0f;
    const double cos_sim = dot / denom;
    return static_cast<float>(std::clamp(1.0 - cos_sim, 0.0, 2.0));
}

}  // namespace

CommunicationMetrics::CommunicationMetrics(std::size_t num_channels, std::size_t action_dim)
    : num_channels_(num_channels), action_dim_(action_dim) {
    per_channel_.resize(num_channels_);
    for (auto& ch : per_channel_) {
        ch.action_sum_active.assign(action_dim_, 0.0);
        ch.action_sum_silent.assign(action_dim_, 0.0);
    }
}

void CommunicationMetrics::recordEmit(std::size_t channel,
                                      AgentId emitter,
                                      SimTimeSeconds emit_time_seconds) {
    if (channel >= num_channels_) return;
    auto& acc = per_channel_[channel];
    ++acc.emit_count;
    acc.unique_emitters.insert(emitter.value);
    acc.recent_emit_times.push_back(emit_time_seconds);
    // Bound the recent_emit_times window to avoid unbounded growth in long
    // episodes; latency is computed against the most-recent emit anyway.
    if (acc.recent_emit_times.size() > 256) {
        acc.recent_emit_times.erase(acc.recent_emit_times.begin(),
                                    acc.recent_emit_times.begin() + 64);
    }
    summarised_ = false;
}

void CommunicationMetrics::recordListenerAction(AgentId listener,
                                                const std::vector<float>& action_vec,
                                                std::uint32_t channel_active_mask,
                                                SimTimeSeconds time_seconds) {
    if (action_vec.size() != action_dim_) return;
    for (std::size_t c = 0; c < num_channels_; ++c) {
        auto& acc = per_channel_[c];
        const bool active = (channel_active_mask & (1u << c)) != 0;
        if (active) {
            acc.unique_listeners.insert(listener.value);
            for (std::size_t i = 0; i < action_dim_; ++i) {
                acc.action_sum_active[i] += action_vec[i];
            }
            ++acc.active_ticks;
            // Compare against this listener's last silent action to count
            // a response_event when the L2 between active and the last
            // silent action exceeds a small threshold.
            auto it_silent = acc.last_silent_action.find(listener.value);
            if (it_silent != acc.last_silent_action.end()) {
                double d2 = 0.0;
                for (std::size_t i = 0; i < action_dim_; ++i) {
                    const double diff = static_cast<double>(action_vec[i]) - static_cast<double>(it_silent->second[i]);
                    d2 += diff * diff;
                }
                if (d2 > 1.0e-4) ++acc.response_events;
            }
            // Response latency: time from most-recent emit on this channel
            // to this listener_tick. Only counted once per emit per
            // listener? Simpler: average over all listener-ticks within
            // 5 seconds of an emit (latency-from-emit approximation).
            if (!acc.recent_emit_times.empty()) {
                const SimTimeSeconds last_emit = acc.recent_emit_times.back();
                const SimTimeSeconds delta = time_seconds - last_emit;
                if (delta >= 0.0 && delta <= 5.0) {
                    acc.latency_sum_seconds += static_cast<double>(delta);
                    ++acc.latency_sample_count;
                }
            }
        } else {
            for (std::size_t i = 0; i < action_dim_; ++i) {
                acc.action_sum_silent[i] += action_vec[i];
            }
            ++acc.silent_ticks;
            acc.last_silent_action[listener.value] = action_vec;
        }
    }
    summarised_ = false;
}

void CommunicationMetrics::summarise(std::size_t generation_index) {
    rows_.clear();
    rows_.reserve(num_channels_);
    for (std::size_t c = 0; c < num_channels_; ++c) {
        const auto& acc = per_channel_[c];
        CommunicationChannelRow r;
        r.generation = generation_index;
        r.channel = c;
        r.emit_count = acc.emit_count;
        r.unique_emitter_count = acc.unique_emitters.size();
        r.unique_listener_count = acc.unique_listeners.size();
        r.listener_ticks = acc.active_ticks;
        r.silent_ticks = acc.silent_ticks;
        r.response_events = acc.response_events;
        r.listener_policy_shift_l2 = l2(acc.action_sum_active, acc.active_ticks,
                                         acc.action_sum_silent, acc.silent_ticks);
        r.listener_policy_shift_cos = cosineDistance(acc.action_sum_active, acc.active_ticks,
                                                      acc.action_sum_silent, acc.silent_ticks);
        if (acc.latency_sample_count > 0) {
            r.response_latency_avg_seconds = static_cast<float>(
                acc.latency_sum_seconds / static_cast<double>(acc.latency_sample_count));
            r.latency_sample_count = acc.latency_sample_count;
        } else {
            r.response_latency_avg_seconds = std::numeric_limits<float>::quiet_NaN();
            r.latency_sample_count = 0;
        }
        rows_.push_back(r);
    }
    summarised_ = true;
}

void CommunicationMetrics::appendCsv(const std::filesystem::path& path, std::string_view tag) const {
    // D-044 disk-budget guard - mirrors CreditLedger::appendCsv. 200 MB
    // per-file ceiling so a single run can't run away with disk.
    constexpr std::uintmax_t kCapBytes = 200ull * 1024ull * 1024ull;
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    const auto current_size = std::filesystem::file_size(path, ec);
    if (!ec && current_size >= kCapBytes) {
        static std::set<std::filesystem::path> warned;
        if (warned.insert(path).second) {
            LOG_WARN("CommunicationMetrics::appendCsv refusing to grow {} past {} MB cap",
                     path.string(), kCapBytes / (1024ull * 1024ull));
        }
        return;
    }
    const bool need_header = !std::filesystem::exists(path, ec)
                             || std::filesystem::file_size(path, ec) == 0;
    std::ofstream out(path, std::ios::app);
    if (!out.is_open()) return;
    if (need_header) {
        out << "scenario_name,generation,channel,emit_count,unique_emitter_count,"
               "unique_listener_count,listener_ticks,silent_ticks,response_events,"
               "listener_policy_shift_l2,listener_policy_shift_cos,"
               "response_latency_avg_seconds,latency_sample_count\n";
    }
    for (const auto& r : rows_) {
        out << tag << ','
            << r.generation << ','
            << r.channel << ','
            << r.emit_count << ','
            << r.unique_emitter_count << ','
            << r.unique_listener_count << ','
            << r.listener_ticks << ','
            << r.silent_ticks << ','
            << r.response_events << ','
            << r.listener_policy_shift_l2 << ','
            << r.listener_policy_shift_cos << ',';
        if (std::isnan(r.response_latency_avg_seconds)) {
            out << "NaN";
        } else {
            out << r.response_latency_avg_seconds;
        }
        out << ',' << r.latency_sample_count << '\n';
    }
}

void CommunicationMetrics::clear() {
    for (auto& ch : per_channel_) {
        ch.emit_count = 0;
        ch.unique_emitters.clear();
        ch.unique_listeners.clear();
        std::fill(ch.action_sum_active.begin(), ch.action_sum_active.end(), 0.0);
        std::fill(ch.action_sum_silent.begin(), ch.action_sum_silent.end(), 0.0);
        ch.active_ticks = 0;
        ch.silent_ticks = 0;
        ch.recent_emit_times.clear();
        ch.latency_sum_seconds = 0.0;
        ch.latency_sample_count = 0;
        ch.last_silent_action.clear();
        ch.response_events = 0;
    }
    rows_.clear();
    summarised_ = false;
}

}  // namespace neuro::routes::protagonist
