#include "core/observer/AdvancedStatsObserver.h"

#include "core/agent/Agent.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <map>
#include <numeric>
#include <unordered_set>
#include <vector>

namespace neuro {

AdvancedStatsObserver::AdvancedStatsObserver(const std::filesystem::path& output_file)
    : output_file_(output_file), world_(nullptr), subscription_id_(0) {}

AdvancedStatsObserver::~AdvancedStatsObserver() {
    if (stream_.is_open()) {
        stream_.flush();
        stream_.close();
    }
}

void AdvancedStatsObserver::onAttach(IWorld& world) {
    world_ = &world;
    std::filesystem::create_directories(output_file_.parent_path());

    const bool exists = std::filesystem::exists(output_file_);
    stream_.open(output_file_, std::ios::out | (exists ? std::ios::app : std::ios::trunc));
    if (!stream_.is_open()) {
        throw std::runtime_error("Failed to open advanced stats output file");
    }

    // Write header if new file
    if (!exists || std::filesystem::file_size(output_file_) == 0) {
        stream_ << "generation,shannon_diversity,fitness_variance,avg_age,avg_exploration,behavior_complexity,species_count,alive_ratio\n";
        stream_.flush();
    }

    subscription_id_ = world.eventBus().subscribe(events::EventType::GenerationEnded,
        [this](const events::Event& event) {
            const auto& payload = event.payloadAs<events::GenerationEnded>();
            onGenerationEnded(payload.gen);
        });
}

void AdvancedStatsObserver::onDetach(IWorld& world) {
    if (subscription_id_ != 0) {
        world.eventBus().unsubscribe(subscription_id_);
        subscription_id_ = 0;
    }
    world_ = nullptr;
    if (stream_.is_open()) {
        stream_.flush();
        stream_.close();
    }
}

const char* AdvancedStatsObserver::name() const {
    return "advanced_stats";
}

double AdvancedStatsObserver::computeShannonDiversity(const std::vector<double>& species_counts) const {
    double total = 0.0;
    for (auto c : species_counts) total += c;
    if (total < 1e-12) return 0.0;

    double entropy = 0.0;
    for (auto c : species_counts) {
        if (c < 1e-12) continue;
        const double p = c / total;
        entropy -= p * std::log2(p);
    }
    return entropy;
}

double AdvancedStatsObserver::computeBehaviorComplexity(const std::vector<int>& action_sequence) const {
    if (action_sequence.size() < 4) return 0.0;

    // Simplified Lempel-Ziv complexity: count distinct substrings
    std::unordered_set<std::size_t> seen;
    std::size_t complexity = 0;
    std::size_t max_len = std::min(action_sequence.size(), static_cast<std::size_t>(16));

    for (std::size_t i = 0; i < action_sequence.size(); ++i) {
        for (std::size_t len = 1; len <= max_len && i + len <= action_sequence.size(); ++len) {
            // Hash the substring
            std::size_t h = 0;
            for (std::size_t j = 0; j < len; ++j) {
                h = h * 31 + static_cast<std::size_t>(action_sequence[i + j]);
            }
            if (seen.find(h) == seen.end()) {
                seen.insert(h);
                complexity++;
            }
        }
    }

    // Normalize by sequence length
    return static_cast<double>(complexity) / static_cast<double>(action_sequence.size());
}

double AdvancedStatsObserver::computeFitnessVariance(const std::vector<double>& fitnesses) const {
    if (fitnesses.size() < 2) return 0.0;
    const double mean = std::accumulate(fitnesses.begin(), fitnesses.end(), 0.0) / static_cast<double>(fitnesses.size());
    double var = 0.0;
    for (auto f : fitnesses) {
        const double d = f - mean;
        var += d * d;
    }
    return var / static_cast<double>(fitnesses.size());
}

void AdvancedStatsObserver::onGenerationEnded(Generation generation) {
    if (world_ == nullptr) return;

    const auto* agent_layer = world_->getLayer<AgentLayer>();
    if (agent_layer == nullptr) return;

    // Collect per-species member counts
    std::map<std::uint64_t, double> species_counts;
    std::vector<double> fitnesses;
    double total_age = 0.0;
    double total_exploration = 0.0;
    double total_complexity = 0.0;
    std::size_t agent_count = 0;
    std::vector<int> action_sequence;

    for (const Agent* agent : agent_layer->agents()) {
        const SpeciesId sid = agent->speciesId();
        if (sid != kInvalidSpeciesId) {
            species_counts[sid.value] += 1.0;
        }
        fitnesses.push_back(agent->fitness());
        total_age += static_cast<double>(agent->age());
        total_exploration += static_cast<double>(agent->visitedCellCount());
        agent_count++;

        // Encode action as integer for LZ complexity
        // Use movement quadrant as action: 0-3 + stationary=4
        const Vec2 vel = agent->velocity();
        const float speed = std::sqrt(vel.x * vel.x + vel.y * vel.y);
        if (speed < 0.01f) {
            action_sequence.push_back(4);
        } else if (vel.x >= 0 && vel.y >= 0) {
            action_sequence.push_back(0);
        } else if (vel.x < 0 && vel.y >= 0) {
            action_sequence.push_back(1);
        } else if (vel.x < 0 && vel.y < 0) {
            action_sequence.push_back(2);
        } else {
            action_sequence.push_back(3);
        }
    }

    // Compute metrics
    std::vector<double> sp_counts;
    for (const auto& [_, c] : species_counts) sp_counts.push_back(c);

    const double shannon = computeShannonDiversity(sp_counts);
    const double fitness_var = computeFitnessVariance(fitnesses);
    const double avg_age = agent_count > 0 ? total_age / agent_count : 0.0;
    const double avg_exploration = agent_count > 0 ? total_exploration / agent_count : 0.0;
    const double behavior_cx = computeBehaviorComplexity(action_sequence);
    const double alive_ratio = agent_count > 0 ? static_cast<double>(agent_layer->livingCount()) / agent_count : 0.0;

    stream_ << generation << ','
            << shannon << ','
            << fitness_var << ','
            << avg_age << ','
            << avg_exploration << ','
            << behavior_cx << ','
            << species_counts.size() << ','
            << alive_ratio << '\n';
    stream_.flush();
}

}  // namespace neuro
