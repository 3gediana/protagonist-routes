#pragma once

#include <array>
#include <cstdint>

namespace dp::agent {

// SpatialMemory · the agent's "cognitive map".
//
// Why this exists: a pure MLP/GRU PPO policy can only "see" within
// VISION_RANGE (60m) for the *current* tick, and a GRU's effective
// memory window is ~3-10 seconds (100-300 ticks at dt=1/30s). On a
// 1024m × 1024m map that means the agent forgets where the river is
// long before it gets thirsty again.
//
// Inspired by Parisotto & Salakhutdinov 2017 "Neural Map: Structured
// Memory for Deep Reinforcement Learning", which proved this kind of
// world-fixed spatial grid beats DNC and matches/exceeds LSTM on 2D/3D
// navigation tasks while staying simple and stable to train.
//
// Conventions:
//   * The grid is world-fixed and large enough to cover the whole map.
//     1024m world @ 16m/cell -> 64x64 cells.
//   * Each cell stores a small fixed feature vector. Currently 6:
//       0: visited     (0 = never stepped on, 1 = visited)
//       1: water       (max-seen "this cell or a neighbour is river/swamp")
//       2: food        (max-seen "there was a ripe plant near here")
//       3: resource    (max-seen "wood/stone/grass nearby")
//       4: danger      (max-seen "a wolf was perceived from here")
//       5: building    (max-seen "we placed/finished a building near here")
//   * We OR-accumulate features: once an interesting thing is observed
//     at a cell, that cell stays "marked" for the rest of the episode.
//     This matches the "episodic memory" interpretation - the agent
//     remembers what it has seen *this life*.
//   * On reset() the grid is zeroed (a new life means re-exploring).
//   * read_window() returns an axis-aligned WINDOW x WINDOW patch
//     centred on the agent's current cell, flattened in row-major
//     (gy, gx, feature) order. Out-of-bounds cells emit zeros.
class SpatialMemory {
public:
    static constexpr int   GRID         = 64;
    static constexpr float CELL_SIZE_M  = 16.0f;
    static constexpr int   FEATURES     = 6;
    static constexpr int   WINDOW       = 8;
    static constexpr int   WINDOW_DIM   = WINDOW * WINDOW * FEATURES;  // 384
    static constexpr int   CELLS        = GRID * GRID;
    static constexpr int   TOTAL_FLOATS = CELLS * FEATURES;

    // Feature channel indices (use these instead of magic numbers).
    enum Channel : int {
        CH_VISITED  = 0,
        CH_WATER    = 1,
        CH_FOOD     = 2,
        CH_RESOURCE = 3,
        CH_DANGER   = 4,
        CH_BUILDING = 5,
    };

    // Bind the grid to world coordinates. Origin defaults to (0,0) which
    // covers a 1024m × 1024m world that starts at the origin (matches
    // World::Config defaults).
    void configure(float origin_x = 0.0f, float origin_y = 0.0f);

    // Wipe the grid. Called at the start of every episode.
    void clear();

    // Write into the cell that contains (wx, wy). Each `saw_*` flag, if
    // true, sets the corresponding channel to 1.0 (max-accumulate).
    // visited is always set to 1 for the cell the agent stands on.
    void write(float wx, float wy,
               bool saw_water,
               bool saw_food,
               bool saw_resource,
               bool saw_danger,
               bool saw_building);

    // Read WINDOW x WINDOW patch centred on (wx, wy). out must have
    // capacity WINDOW_DIM (=384). Stored in row-major (gy, gx, feature).
    void read_window(float wx, float wy, float* out) const;

    // Diagnostics
    int   visited_count() const;
    float feature_avg(Channel c) const;
    const std::array<float, TOTAL_FLOATS>& raw() const { return cells_; }

private:
    bool world_to_grid_(float wx, float wy, int& gx, int& gy) const;

    float                                    origin_x_ = 0.0f;
    float                                    origin_y_ = 0.0f;
    std::array<float, TOTAL_FLOATS>          cells_{};
};

}  // namespace dp::agent
