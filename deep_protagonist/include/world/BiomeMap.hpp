#pragma once

#include "world/Heightmap.hpp"

#include <cstdint>
#include <vector>

namespace dp::world {

// Biome classification: which kind of natural environment a given world cell
// belongs to. Decided by a 2D function of height and "moisture" (a second
// noise field). Numbers are stable - they're persisted to disk.
enum class Biome : uint8_t {
    Plain    = 0,
    Forest   = 1,
    Mountain = 2,
    River    = 3,
    Swamp    = 4,
    Desert   = 5,
    COUNT
};

const char* biome_name(Biome b);

// Per-cell biome classification covering the same grid as the heightmap.
// Cells are determined from a moisture noise + the heightmap; near-river
// cells are forced to River/Swamp by a separate river path mask layer.
class BiomeMap {
public:
    BiomeMap(int size_x, int size_y);

    int size_x() const { return size_x_; }
    int size_y() const { return size_y_; }

    Biome at(int i, int j) const;
    Biome sample(float wx, float wy, float cell_scale) const;

    // Build the biome map from terrain data. The moisture noise is generated
    // internally with the supplied seed.
    void build(const Heightmap& hm, uint64_t seed,
               float sea_level = 1.5f,
               float mountain_threshold = 0.65f,
               float wet_threshold = 0.55f,
               float dry_threshold = 0.30f);

    // Mark a cell as river/swamp (used by the river system to carve paths).
    void mark_river(int i, int j);
    void mark_swamp(int i, int j);

    const std::vector<uint8_t>& raw() const { return data_; }

private:
    int size_x_;
    int size_y_;
    std::vector<uint8_t> data_;  // row-major, one Biome per cell

    inline int idx(int i, int j) const { return i + j * size_x_; }
};

}  // namespace dp::world
