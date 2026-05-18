#pragma once

#include "core/interfaces/IObserver.h"

#include <filesystem>
#include <fstream>

namespace neuro {

class IWorld;

class SpeciesObserver final : public IObserver {
public:
    explicit SpeciesObserver(const std::filesystem::path& output_file);
    ~SpeciesObserver() override;

    void onAttach(IWorld& world) override;
    void onDetach(IWorld& world) override;
    const char* name() const override;

private:
    void writeCurrentGenerationSnapshot(Generation generation);

    std::filesystem::path output_file_;
    std::ofstream stream_;
    IWorld* world_;
    events::SubscriptionId subscription_id_;
};

}  // namespace neuro
