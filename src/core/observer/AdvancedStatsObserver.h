#pragma once

#include "core/interfaces/IObserver.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <vector>

namespace neuro {

class IWorld;

class AdvancedStatsObserver final : public IObserver {
public:
    explicit AdvancedStatsObserver(const std::filesystem::path& output_file);
    ~AdvancedStatsObserver() override;

    void onAttach(IWorld& world) override;
    void onDetach(IWorld& world) override;
    const char* name() const override;

private:
    void onGenerationEnded(Generation generation);
    double computeShannonDiversity(const std::vector<double>& species_counts) const;
    double computeBehaviorComplexity(const std::vector<int>& action_sequence) const;
    double computeFitnessVariance(const std::vector<double>& fitnesses) const;
    std::filesystem::path output_file_;
    std::ofstream stream_;
    IWorld* world_;
    events::SubscriptionId subscription_id_;
};

}  // namespace neuro
