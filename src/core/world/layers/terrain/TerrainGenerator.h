#pragma once

#include "core/world/layers/terrain/HeightField.h"
#include "core/world/layers/terrain/CaveMask.h"

#include <cstdint>
#include <cstddef>

namespace neuro {

struct TerrainParams {
    float height_scale = 30.0f;
    float slope_resistance = 0.6f;
    float cave_density = 0.05f;
    float cave_shelter_factor = 0.5f;
    float min_velocity_factor = 0.1f;
    std::uint32_t seed = 42;
    std::size_t grid_resolution = 64;
    float perlin_frequency = 4.0f;
    float perlin_persistence = 0.5f;
    int perlin_octaves = 4;
};

class TerrainGenerator {
public:
    static void generateHeightField(HeightField& hf, const TerrainParams& params, std::uint32_t seed);
    static void generateCaveMask(CaveMask& mask, const TerrainParams& params, std::uint32_t seed);
};

}  // namespace neuro
