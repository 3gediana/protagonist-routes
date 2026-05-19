#pragma once

#include "core/interfaces/IObserver.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <vector>

namespace neuro {

class IWorld;

class HeatmapObserver final : public IObserver {
public:
    HeatmapObserver(std::size_t grid_resolution,
                    std::size_t sample_interval,
                    const std::filesystem::path& output_dir);

    ~HeatmapObserver() override;

    void onAttach(IWorld& world) override;
    void onDetach(IWorld& world) override;
    const char* name() const override;

private:
    void onTick();
    void onGenerationEnded(Generation generation);
    void writeHeatmapCsv(Generation generation, const std::vector<float>& grid,
                         const std::string& suffix);

    std::size_t grid_resolution_;
    std::size_t sample_interval_;
    std::filesystem::path output_dir_;

    // Per-category heatmaps: "all", "predator", "prey", "species_N"
    std::unordered_map<std::string, std::vector<float>> heatmaps_;
    std::unordered_map<std::string, std::vector<float>> cumulative_;

    IWorld* world_;
    events::SubscriptionId tick_sub_id_;
    events::SubscriptionId gen_sub_id_;
    std::size_t tick_counter_;

    void ensureHeatmap(const std::string& key);
};

}  // namespace neuro
