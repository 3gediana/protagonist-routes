#pragma once

#include "world/BiomeMap.hpp"
#include "world/CaveSystem.hpp"
#include "world/Heightmap.hpp"
#include "world/RiverSystem.hpp"

#include <cstdint>
#include <memory>

namespace dp::world {

// Aggregates all static world geometry. The world owns the heightmap, biome
// map, river system and caves. Agents live elsewhere.
class World {
public:
    struct Config {
        // Default: 256 cells * 4m/cell = 1024m world. Mesh complexity stays
        // the same as a 256m world but coverage is 16x larger.
        int   heightmap_cells_x = 256;
        int   heightmap_cells_y = 256;
        float cell_scale        = 4.0f;     // world meters per cell
        float terrain_max_height = 120.0f;  // real relief: ~100m+ summits in a 1km world
        float terrain_frequency  = 0.0015f; // smaller -> bigger features
        int   terrain_octaves    = 6;
        float terrain_persistence = 0.5f;
        int   cave_count         = 6;       // more caves in a 1km world
        int   river_count        = 3;
        uint64_t seed            = 12345;
    };

    explicit World(const Config& cfg);

    const Heightmap&   heightmap()  const { return *heightmap_; }
    const BiomeMap&    biomes()     const { return *biomes_; }
    const RiverSystem& rivers()     const { return rivers_; }
    const CaveSystem&  caves()      const { return caves_; }
    const Config&      config()     const { return cfg_; }

    // Surface height at (wx, wy). NOT including caves.
    float surface_height(float wx, float wy) const {
        return heightmap_->sample(wx, wy);
    }

    Biome biome_at(float wx, float wy) const {
        return biomes_->sample(wx, wy, cfg_.cell_scale);
    }

    bool is_underground(float wx, float wy, float wz) const {
        return caves_.point_inside(wx, wy, wz);
    }

private:
    Config cfg_;
    std::unique_ptr<Heightmap> heightmap_;
    std::unique_ptr<BiomeMap>  biomes_;
    RiverSystem rivers_;
    CaveSystem  caves_;
};

}  // namespace dp::world
