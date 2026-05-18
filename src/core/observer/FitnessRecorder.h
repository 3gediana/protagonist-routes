#pragma once

#include "core/interfaces/IObserver.h"

#include <filesystem>
#include <fstream>

namespace neuro {

class FitnessRecorder final : public IObserver {
public:
    explicit FitnessRecorder(const std::filesystem::path& output_file);
    ~FitnessRecorder() override;

    void onAttach(IWorld& world) override;
    void onDetach(IWorld& world) override;
    const char* name() const override;

private:
    std::filesystem::path output_file_;
    std::ofstream stream_;
    events::SubscriptionId subscription_id_;
};

}  // namespace neuro
