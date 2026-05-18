#pragma once

#include "core/interfaces/IObserver.h"

#include <chrono>
#include <filesystem>
#include <fstream>

namespace neuro {

class PerformanceObserver final : public IObserver {
public:
    explicit PerformanceObserver(const std::filesystem::path& output_file);
    ~PerformanceObserver() override;

    void onAttach(IWorld& world) override;
    void onDetach(IWorld& world) override;
    const char* name() const override;

    void markGenerationStart();
    void markEvaluationStart();
    void markEvaluationEnd();
    void markReproductionStart();
    void markReproductionEnd();
    void finishGeneration(Generation generation);

private:
    void writeRecord(Generation generation);

    std::filesystem::path output_file_;
    std::ofstream stream_;
    IWorld* world_;
    events::SubscriptionId subscription_id_;

    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    TimePoint gen_start_;
    TimePoint eval_start_;
    TimePoint repro_start_;
    double eval_ms_{0.0};
    double repro_ms_{0.0};
};

}  // namespace neuro
