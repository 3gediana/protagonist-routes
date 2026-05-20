#pragma once

#include "core/interfaces/IObserver.h"
#include "core/pathogen/PathogenGenome.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <unordered_map>
#include <vector>

namespace neuro {

class IWorld;

// Comprehensive ecosystem metrics observer.
// Tracks: OEE complexity, lineage, information-theoretic, population-level stats.
class ComprehensiveMetrics final : public IObserver {
public:
    explicit ComprehensiveMetrics(const std::filesystem::path& output_dir);
    ~ComprehensiveMetrics() override;

    void onAttach(IWorld& world) override;
    void onDetach(IWorld& world) override;
    const char* name() const override { return "comprehensive_metrics"; }

private:
    static constexpr std::size_t kActionBuckets = 256;

    void onGenerationEnded(Generation generation);
    void onTickEnded();

    IWorld* world_ = nullptr;
    std::filesystem::path output_dir_;
    events::SubscriptionId gen_sub_id_{0};
    events::SubscriptionId tick_sub_id_{0};
    events::SubscriptionId any_sub_id_{0};

    // --- Per-tick sampling ---
    std::size_t tick_count_ = 0;
    // Signal-field total energy per tick (for entropy computation)
    std::vector<double> signal_energy_history_;
    // Agent position entropy bins
    static constexpr std::size_t kSpatialBins = 16;
    std::vector<double> spatial_entropy_accum_;  // accumulated over ticks in this gen
    std::unordered_map<AgentId, std::uint16_t> tick_action_flags_;
    std::vector<double> action_bucket_accum_;
    std::vector<double> signal_action_joint_accum_;
    std::vector<double> terrain_action_joint_accum_;
    std::vector<double> infection_action_joint_accum_;
    double disease_virulence_sum_ = 0.0;
    double disease_max_virulence_ = 0.0;
    double disease_immunity_rate_sum_ = 0.0;
    double disease_immune_memory_sum_ = 0.0;
    std::size_t disease_alive_agent_samples_ = 0;
    std::size_t disease_infected_agent_samples_ = 0;
    std::map<PathogenGenome::StrainSignature, std::size_t> disease_strain_counts_;

    // --- Per-generation accumulators ---
    std::size_t gen_ = 0;

    // OEE: network complexity
    struct NetworkStats {
        double avg_nodes = 0;
        double avg_connections = 0;
        double avg_hidden = 0;
        double max_nodes = 0;
        double max_connections = 0;
    };
    NetworkStats last_network_stats_;

    // Lineage tracking
    // Population-level stats
    struct PopStats {
        double shannon_diversity = 0;
        double species_count = 0;
        double avg_species_size = 0;
        double max_species_size = 0;
        double species_size_entropy = 0;  // entropy of species size distribution
        double spatial_entropy = 0;       // how spread out agents are
        double behavioral_entropy = 0;    // entropy of action distribution
        double fitness_entropy = 0;       // entropy of fitness distribution
        double avg_energy = 0;
        double energy_variance = 0;
        double alive_ratio = 0;
        double avg_age = 0;
        double pred_count = 0;
        double prey_count = 0;
        double infected_ratio = 0;
        double immune_ratio = 0;
        double carrying_ratio = 0;  // agents carrying objects
        double avg_exploration = 0; // avg unique cells visited
    };
    PopStats last_pop_stats_;

    // Information-theoretic
    struct InfoStats {
        double signal_field_entropy = 0;     // entropy of signal field energy distribution
        double action_entropy = 0;           // entropy of agent action distribution
        double perception_entropy = 0;       // entropy of perception values
        double signal_action_mi = 0;         // MI between signal perception and action
        double terrain_behavior_mi = 0;      // MI between terrain perception and action
        double infection_behavior_mi = 0;    // MI between infection state and action
    };
    InfoStats last_info_stats_;

    // Coevolution
    struct CoevoStats {
        double pred_best_fitness = 0;
        double prey_best_fitness = 0;
        double pred_avg_fitness = 0;
        double prey_avg_fitness = 0;
        double red_queen_corr = 0;
        double hunt_success_rate = 0;
        double pred_prey_size_ratio = 0;
    };
    CoevoStats last_coevo_stats_;

    // Disease arms race
    struct DiseaseStats {
        double avg_virulence = 0;       // avg pathogen NN output
        double max_virulence = 0;
        double strain_count = 0;
        double strain_diversity = 0;    // entropy of strain distribution
        double avg_immunity_rate = 0;
        double immune_memory_size = 0;
        double infection_rate = 0;      // currently infected / alive
    };
    DiseaseStats last_disease_stats_;

    // CSV files
    std::ofstream oee_csv_;       // open-ended evolution complexity
    std::ofstream pop_csv_;       // population statistics
    std::ofstream info_csv_;      // information-theoretic
    std::ofstream coevo_csv_;     // coevolution
    std::ofstream disease_csv_;   // disease arms race
    std::ofstream species_profile_csv_;  // per-species behavioral profile

    // Red Queen sliding window
    static constexpr std::size_t kRedQueenWindow = 50;
    std::vector<double> pred_best_history_;
    std::vector<double> prey_best_history_;

    // Helper
    static double computeEntropy(const std::vector<double>& probs);
    static double computePearson(const std::vector<double>& x, const std::vector<double>& y);
    static double computeMI(const std::vector<std::pair<int,int>>& joint, int kx, int ky);
    static double computeMIFromCounts(const std::vector<double>& joint_counts, int kx, int ky);
};

}  // namespace neuro
