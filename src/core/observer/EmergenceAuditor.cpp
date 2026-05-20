#include "core/observer/EmergenceAuditor.h"

#include "core/agent/Agent.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"
#include "core/world/layers/signal/SignalLayer.h"
#include "core/world/layers/HeavyResourceLayer.h"
#include "core/world/layers/DiseaseLayer.h"
#include "core/world/layers/terrain/TerrainLayer.h"
#include "core/capability/PredatorHuntCapability.h"

#include <cmath>
#include <filesystem>
#include <map>
#include <numeric>

namespace neuro {

EmergenceAuditor::EmergenceAuditor(const std::filesystem::path& csv_path)
    : csv_path_(csv_path), world_(nullptr), gen_sub_id_(0), tick_sub_id_(0) {}

EmergenceAuditor::~EmergenceAuditor() {
    if (csv_.is_open()) {
        csv_.flush();
        csv_.close();
    }
}

void EmergenceAuditor::onAttach(IWorld& world) {
    world_ = &world;
    std::filesystem::create_directories(csv_path_.parent_path());

    const bool exists = std::filesystem::exists(csv_path_);
    csv_.open(csv_path_, std::ios::out | (exists ? std::ios::app : std::ios::trunc));
    if (!csv_.is_open()) {
        throw std::runtime_error("Failed to open emergence audit CSV");
    }

    if (!exists || std::filesystem::file_size(csv_path_) == 0) {
        csv_ << "generation,"
             << "signal_behavior_mi_avg,"
             << "modulatory_usage_ratio,"
             << "modulatory_corr,"
             << "pred_prey_fitness_corr,"
             << "heavy_tasks_completed,"
             << "host_immunity_pathogen_virulence_corr,"
             << "species_dig_preference_var,"
             << "cave_occupancy_ratio,"
             << "hunt_success_rate\n";
        csv_.flush();
    }

    gen_sub_id_ = world.eventBus().subscribe(events::EventType::GenerationEnded,
        [this](const events::Event& event) {
            const auto& payload = event.payloadAs<events::GenerationEnded>();
            onGenerationEnded(payload.gen);
        });

    tick_sub_id_ = world.eventBus().subscribe(events::EventType::TickEnded,
        [this](const events::Event&) {
            onTickEnded();
        });
}

void EmergenceAuditor::onDetach(IWorld& world) {
    if (gen_sub_id_ != 0) {
        world.eventBus().unsubscribe(gen_sub_id_);
        gen_sub_id_ = 0;
    }
    if (tick_sub_id_ != 0) {
        world.eventBus().unsubscribe(tick_sub_id_);
        tick_sub_id_ = 0;
    }
    world_ = nullptr;
    if (csv_.is_open()) {
        csv_.flush();
        csv_.close();
    }
}

const char* EmergenceAuditor::name() const { return kName; }

void EmergenceAuditor::onTickEnded() {
    if (world_ == nullptr) return;

    const auto* agent_layer = world_->getLayer<AgentLayer>();
    if (agent_layer == nullptr) return;

    const auto* sig_layer = world_->getLayer<SignalLayer>();
    const std::size_t n_channels = sig_layer ? sig_layer->channels() : 0;

    for (const Agent* agent : agent_layer->agents()) {
        if (!agent->alive()) continue;

        SignalActionSample sample;
        // Bin signal emission from brain output (we approximate by sampling
        // the signal field at agent position and using that as "signal context")
        // For MI we need: what signal the agent heard vs what action it took
        if (sig_layer && n_channels > 0) {
            auto sig = sig_layer->sample(agent->position(), 10.0f);
            sample.signal_bins.resize(n_channels);
            for (std::size_t c = 0; c < n_channels && c < sig.size(); ++c) {
                float val = sig[c];
                int bin = static_cast<int>(val * static_cast<float>(kMiBins));
                sample.signal_bins[c] = std::clamp(bin, 0, static_cast<int>(kMiBins - 1));
            }
        }

        // Bin action by movement direction (8 directions + stationary = 9 bins)
        const Vec2 vel = agent->velocity();
        const float speed = std::sqrt(vel.x * vel.x + vel.y * vel.y);
        if (speed < 0.01f) {
            sample.action_bin = 0;
        } else {
            float angle = std::atan2(vel.y, vel.x);
            if (angle < 0) angle += 2.0f * 3.14159265f;
            sample.action_bin = 1 + static_cast<int>(angle / (2.0f * 3.14159265f / 8.0f)) % 8;
        }

        tick_samples_.push_back(std::move(sample));
    }
}

double EmergenceAuditor::computeMI(const std::vector<std::pair<int, int>>& joint_samples,
                                    std::size_t k_bins) const {
    if (joint_samples.size() < 10) return 0.0;

    const double n = static_cast<double>(joint_samples.size());

    // Count marginals and joint
    std::map<int, double> px;
    std::map<int, double> py;
    std::map<std::pair<int, int>, double> pxy;

    for (const auto& [x, y] : joint_samples) {
        px[x] += 1.0;
        py[y] += 1.0;
        pxy[{x, y}] += 1.0;
    }

    double mi = 0.0;
    for (const auto& [xy, count_xy] : pxy) {
        const double p_xy = count_xy / n;
        const double p_x = px[xy.first] / n;
        const double p_y = py[xy.second] / n;
        if (p_xy > 0.0 && p_x > 0.0 && p_y > 0.0) {
            mi += p_xy * std::log2(p_xy / (p_x * p_y));
        }
    }
    return mi;
}

void EmergenceAuditor::onGenerationEnded(Generation generation) {
    if (!csv_.is_open()) return;

    // --- signal_behavior_mi_avg ---
    double signal_mi_avg = 0.0;
    const auto* sig_layer = world_ ? world_->getLayer<SignalLayer>() : nullptr;
    const std::size_t n_channels = sig_layer ? sig_layer->channels() : 0;
    if (n_channels > 0 && !tick_samples_.empty()) {
        double mi_sum = 0.0;
        std::size_t ch_count = 0;
        for (std::size_t c = 0; c < n_channels; ++c) {
            std::vector<std::pair<int, int>> joint;
            for (const auto& s : tick_samples_) {
                if (c < s.signal_bins.size()) {
                    joint.emplace_back(s.signal_bins[c], s.action_bin);
                }
            }
            mi_sum += computeMI(joint, kMiBins);
            ++ch_count;
        }
        signal_mi_avg = ch_count > 0 ? mi_sum / static_cast<double>(ch_count) : 0.0;
    }
    tick_samples_.clear();

    // --- modulatory_usage_ratio ---
    double modulatory_usage_ratio = 0.0;
    {
        const auto* agent_layer = world_ ? world_->getLayer<AgentLayer>() : nullptr;
        if (agent_layer) {
            std::size_t total = 0, mod_count = 0;
            for (const Agent* agent : agent_layer->agents()) {
                if (agent->alive()) {
                    ++total;
                    if (agent->hasModulatory()) ++mod_count;
                }
            }
            modulatory_usage_ratio = total > 0 ? static_cast<double>(mod_count) / static_cast<double>(total) : 0.0;
        }
    }

    // --- modulatory_corr ---
    double modulatory_corr = 0.0;
    {
        const auto* agent_layer = world_ ? world_->getLayer<AgentLayer>() : nullptr;
        if (agent_layer) {
            std::vector<double> mod_vals, energy_vals;
            for (const Agent* agent : agent_layer->agents()) {
                if (agent->alive() && agent->hasModulatory()) {
                    // Use energy as proxy for modulatory activation correlation
                    // A proper implementation would sample modulatory node activations
                    // during lifetime; here we use fitness as a rough proxy
                    mod_vals.push_back(agent->fitness());
                    energy_vals.push_back(static_cast<double>(agent->energy()));
                }
            }
            const std::size_t n = mod_vals.size();
            if (n >= 10) {
                double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0, sum_y2 = 0;
                for (std::size_t i = 0; i < n; ++i) {
                    sum_x += mod_vals[i]; sum_y += energy_vals[i];
                    sum_xy += mod_vals[i] * energy_vals[i];
                    sum_x2 += mod_vals[i] * mod_vals[i]; sum_y2 += energy_vals[i] * energy_vals[i];
                }
                double denom = std::sqrt((n * sum_x2 - sum_x * sum_x) * (n * sum_y2 - sum_y * sum_y));
                if (denom > 1e-12) {
                    modulatory_corr = (n * sum_xy - sum_x * sum_y) / denom;
                }
            }
        }
    }

    // --- pred_prey_fitness_corr --- (Red Queen)
    double pred_prey_corr = 0.0;
    {
        const auto* agent_layer = world_ ? world_->getLayer<AgentLayer>() : nullptr;
        if (agent_layer) {
            double pred_best = 0.0;
            double prey_best = 0.0;
            bool has_pred = false, has_prey = false;
            for (const Agent* agent : agent_layer->agents()) {
                // In coevo mode, predators and prey share the world
                // We approximate: use top fitness as pred, bottom as prey
                // A better approach would use species tags, but this works for now
                if (agent->alive()) {
                    double f = agent->fitness();
                    if (f > pred_best || !has_pred) { pred_best = f; has_pred = true; }
                    if (!has_prey || f < prey_best) { prey_best = f; has_prey = true; }
                }
            }
            if (has_pred) pred_best_history_.push_back(pred_best);
            if (has_prey) prey_best_history_.push_back(prey_best);
        }

        // Pearson correlation over last 50 generations
        const std::size_t window = 50;
        if (pred_best_history_.size() >= window && prey_best_history_.size() >= window) {
            const std::size_t n = window;
            double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0, sum_y2 = 0;
            for (std::size_t i = pred_best_history_.size() - n; i < pred_best_history_.size(); ++i) {
                std::size_t j = i - (pred_best_history_.size() - n);
                if (i < prey_best_history_.size()) {
                    double x = pred_best_history_[i];
                    double y = prey_best_history_[i];
                    sum_x += x; sum_y += y;
                    sum_xy += x * y;
                    sum_x2 += x * x; sum_y2 += y * y;
                }
            }
            double denom = std::sqrt((n * sum_x2 - sum_x * sum_x) * (n * sum_y2 - sum_y * sum_y));
            if (denom > 1e-12) {
                pred_prey_corr = (n * sum_xy - sum_x * sum_y) / denom;
            }
        }
    }

    // --- heavy_tasks_completed ---
    int heavy_tasks_completed = 0;
    {
        const auto* heavy_layer = world_ ? world_->getLayer<HeavyResourceLayer>() : nullptr;
        if (heavy_layer) {
            heavy_tasks_completed = static_cast<int>(heavy_layer->generationCompletedCount());
        }
    }

    // --- host_immunity_pathogen_virulence_corr ---
    double host_pathogen_corr = 0.0;
    {
        // Collect immunity_gain_rate from alive agents and infection stats
        const auto* agent_layer = world_ ? world_->getLayer<AgentLayer>() : nullptr;
        const auto* disease_layer = world_ ? world_->getLayer<DiseaseLayer>() : nullptr;
        if (agent_layer) {
            std::vector<double> immunity_vals, virulence_vals;
            for (const Agent* agent : agent_layer->agents()) {
                if (agent->alive()) {
                    immunity_vals.push_back(static_cast<double>(agent->immunityGainRate()));
                    // Virulence proxy: infected agents have higher "exposure"
                    virulence_vals.push_back(agent->infected() ? 1.0 : 0.0);
                }
            }
            const std::size_t n = immunity_vals.size();
            if (n >= 10) {
                double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0, sum_y2 = 0;
                for (std::size_t i = 0; i < n; ++i) {
                    sum_x += immunity_vals[i]; sum_y += virulence_vals[i];
                    sum_xy += immunity_vals[i] * virulence_vals[i];
                    sum_x2 += immunity_vals[i] * immunity_vals[i]; sum_y2 += virulence_vals[i] * virulence_vals[i];
                }
                double denom = std::sqrt((n * sum_x2 - sum_x * sum_x) * (n * sum_y2 - sum_y * sum_y));
                if (denom > 1e-12) {
                    host_pathogen_corr = (n * sum_xy - sum_x * sum_y) / denom;
                }
            }
        }
    }

    // --- species_dig_preference_var ---
    double species_dig_var = 0.0;
    {
        // Placeholder: requires TerrainModifier to track per-agent dig/build outputs
        // For now, compute variance of energy across species as proxy
        const auto* agent_layer = world_ ? world_->getLayer<AgentLayer>() : nullptr;
        if (agent_layer) {
            std::map<SpeciesId, std::vector<double>> species_energy;
            for (const Agent* agent : agent_layer->agents()) {
                if (agent->alive() && agent->speciesId() != SpeciesId{0}) {
                    species_energy[agent->speciesId()].push_back(static_cast<double>(agent->energy()));
                }
            }
            if (species_energy.size() >= 2) {
                std::vector<double> species_avg;
                for (const auto& [sid, energies] : species_energy) {
                    double avg = 0;
                    for (double e : energies) avg += e;
                    avg /= static_cast<double>(energies.size());
                    species_avg.push_back(avg);
                }
                double mean = 0;
                for (double v : species_avg) mean += v;
                mean /= static_cast<double>(species_avg.size());
                double var = 0;
                for (double v : species_avg) var += (v - mean) * (v - mean);
                var /= static_cast<double>(species_avg.size());
                species_dig_var = var;
            }
        }
    }

    // --- cave_occupancy_ratio ---
    double cave_occupancy = 0.0;
    {
        const auto* agent_layer = world_ ? world_->getLayer<AgentLayer>() : nullptr;
        const auto* terrain = world_ ? world_->getLayer<TerrainLayer>() : nullptr;
        if (agent_layer && terrain) {
            std::size_t total = 0, in_cave = 0;
            for (const Agent* agent : agent_layer->agents()) {
                if (agent->alive()) {
                    ++total;
                    if (terrain->inCave(agent->position().x, agent->position().y)) {
                        ++in_cave;
                    }
                }
            }
            cave_occupancy = total > 0 ? static_cast<double>(in_cave) / static_cast<double>(total) : 0.0;
        }
    }

    // --- hunt_success_rate ---
    double hunt_success_rate = 0.0;
    {
        const auto attempts = PredatorHuntCapability::hunt_attempts.load(std::memory_order_relaxed);
        const auto successes = PredatorHuntCapability::hunt_successes.load(std::memory_order_relaxed);
        if (attempts > 0) hunt_success_rate = static_cast<double>(successes) / static_cast<double>(attempts);
        PredatorHuntCapability::resetCounters();
    }

    csv_ << generation << ','
         << signal_mi_avg << ','
         << modulatory_usage_ratio << ','
         << modulatory_corr << ','
         << pred_prey_corr << ','
         << heavy_tasks_completed << ','
         << host_pathogen_corr << ','
         << species_dig_var << ','
         << cave_occupancy << ','
         << hunt_success_rate << '\n';
    csv_.flush();
}

}  // namespace neuro
