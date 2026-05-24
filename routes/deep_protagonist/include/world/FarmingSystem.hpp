#pragma once

#include "world/PlantSystem.hpp"  // for PlantKind

#include <cstdint>
#include <vector>

namespace dp::world {

// A single planted patch. Growth ticks up while water_level > 0; when
// growth reaches 1.0 the plot is "ripe" and can be harvested.
struct FarmPlot {
    float x, y, z;
    PlantKind seed_kind;       // matches the harvested produce
    float growth      = 0.0f;  // [0, 1]
    float water_level = 0.5f;  // [0, 1]; gets drained over time
    bool  ripe        = false;
    bool  harvested   = false;
};

class FarmingSystem {
public:
    // Plant a seed at (x,y,z). The plot is created in dry-ish state and
    // will need water to grow.
    int  plant_seed(float x, float y, float z, PlantKind kind);

    // Sprinkle water on the closest plot within reach. Returns true if
    // any plot was watered.
    bool water_at(float x, float y, float reach = 2.0f);

    // Pick up ripe plots near (x,y). Returns the kinds harvested; the
    // matching plots are marked harvested (and removed on next tick).
    std::vector<PlantKind> harvest_at(float x, float y, float reach = 2.0f);

    // Advance growth + drain water + dispose of harvested plots.
    void tick(float dt);

    const std::vector<FarmPlot>& plots() const { return plots_; }

    int ripe_count() const;
    int total_count() const { return int(plots_.size()); }

    // Episode reset: drop all plots.
    void clear();

    // Tunables
    float growth_per_second = 1.0f / 120.0f;  // 2 minutes to mature with full water
    float water_drain_per_second = 1.0f / 60.0f;  // dry in 1 minute
    float water_step = 0.5f;                  // each watering adds 0.5

private:
    std::vector<FarmPlot> plots_;
};

}  // namespace dp::world
