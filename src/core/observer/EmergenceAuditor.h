#pragma once

#include "core/interfaces/IObserver.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <vector>

namespace neuro {

class IWorld;

class EmergenceAuditor final : public IObserver {
public:
    static constexpr const char* kName = "emergence_auditor";
    static constexpr std::size_t kMiBins = 8;

    explicit EmergenceAuditor(const std::filesystem::path& csv_path);
    ~EmergenceAuditor() override;

    void onAttach(IWorld& world) override;
    void onDetach(IWorld& world) override;
    const char* name() const override;

private:
    void onTickEnded();
    void onGenerationEnded(Generation generation);

    double computeMI(const std::vector<std::pair<int, int>>& joint_samples,
                     std::size_t k_bins) const;

    struct SignalActionSample {
        std::vector<int> signal_bins;
        int action_bin;
    };
    std::vector<SignalActionSample> tick_samples_;

    std::vector<double> pred_best_history_;
    std::vector<double> prey_best_history_;

    std::filesystem::path csv_path_;
    std::ofstream csv_;
    IWorld* world_;
    events::SubscriptionId gen_sub_id_;
    events::SubscriptionId tick_sub_id_;
};

}  // namespace neuro
