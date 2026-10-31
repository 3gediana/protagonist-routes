#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace neuro {

class HeightField;

// Discrete ecology classes, mirroring the deep_protagonist BiomeMap. Biomes are
// derived from a Whittaker-style climate model (elevation + temperature +
// moisture) sliced by PERCENTILE, so the biome MIX stays diverse and stable on
// every seed while each biome forms contiguous regions scattered across the
// WHOLE map (not one central pocket). Resource layers query this map so that
// trees/water follow the ecology instead of ringing the base.
enum class Biome : std::uint8_t {
    Plain = 0,
    Forest,
    Mountain,
    River,
    Swamp,
    Desert,
};

const char* biome_name(Biome b);

class BiomeMap {
public:
    BiomeMap() = default;
    BiomeMap(std::size_t size_x, std::size_t size_y, float cell_scale);

    // Build the biome grid from a height field. Cells are aligned 1:1 with the
    // height field grid (same resolution + cell scale).
    void build(const HeightField& hm, std::uint64_t seed);

    bool valid() const { return size_x_ > 0 && size_y_ > 0; }
    std::size_t sizeX() const { return size_x_; }
    std::size_t sizeY() const { return size_y_; }

    Biome at(int i, int j) const;
    Biome sample(float wx, float wy) const;

    // Placement suitability in [0,1] for a world position. Trees concentrate in
    // forest (and a little in swamp/plain); water sits in river/swamp/low basins.
    // Resource layers use these as accept probabilities for rejection sampling.
    float treeSuitability(float wx, float wy) const;
    float waterSuitability(float wx, float wy) const;

    static float treeWeight(Biome b);
    static float waterWeight(Biome b);

private:
    std::size_t idx(int i, int j) const {
        return static_cast<std::size_t>(j) * size_x_ + static_cast<std::size_t>(i);
    }

    std::size_t size_x_ = 0;
    std::size_t size_y_ = 0;
    float cell_scale_ = 1.0f;
    std::vector<std::uint8_t> data_;
};

}  // namespace neuro
