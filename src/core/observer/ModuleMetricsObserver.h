#pragma once

#include "core/interfaces/IObserver.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>

namespace neuro {

class ModuleMetricsObserver final : public IObserver {
public:
    explicit ModuleMetricsObserver(const std::filesystem::path& csv_path);

    void onAttach(IWorld& world) override;
    void onDetach(IWorld& world) override;
    const char* name() const override;
    void onTick(IWorld& world);

    static constexpr const char* kName = "module_metrics";

private:
    void resetAccumulators();

    std::ofstream csv_;
    std::size_t generation_{0};

    // Tick-level extremum trackers
    std::size_t max_infected_{0};
    float min_light_{2.0f};
    float max_light_{-1.0f};
    std::size_t min_season_{999};
    std::size_t max_season_{0};

    // Behavioral accumulators (summed over ticks, averaged on detach)
    double speed_sum_{0.0};
    double infected_speed_sum_{0.0};
    std::size_t infected_speed_ticks_{0};
    double healthy_infected_dist_sum_{0.0};
    std::size_t healthy_dist_ticks_{0};
    std::size_t tick_count_{0};
};

}  // namespace neuro

