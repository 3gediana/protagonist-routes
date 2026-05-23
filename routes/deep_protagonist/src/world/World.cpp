#include "world/World.hpp"

namespace dp::world {

World::World(const Config& cfg) : cfg_(cfg) {
    // 1) Terrain
    heightmap_ = std::make_unique<Heightmap>(cfg.heightmap_cells_x,
                                             cfg.heightmap_cells_y,
                                             cfg.cell_scale);
    heightmap_->fill_fractal_noise(cfg.seed,
                                   cfg.terrain_max_height,
                                   cfg.terrain_frequency,
                                   cfg.terrain_octaves,
                                   cfg.terrain_persistence);

    // 2) Initial biome classification (rivers will mutate this)
    biomes_ = std::make_unique<BiomeMap>(cfg.heightmap_cells_x,
                                         cfg.heightmap_cells_y);
    biomes_->build(*heightmap_, cfg.seed ^ 0xAAAAULL);

    // 3) Carve rivers (mutates both heightmap and biomes)
    rivers_.generate(cfg.seed ^ 0xBBBBULL, cfg.river_count, *heightmap_, *biomes_);

    // 4) Caves
    caves_.generate_random(cfg.seed ^ 0xCAFEBABEULL,
                           cfg.cave_count,
                           heightmap_->world_size_x(),
                           heightmap_->world_size_y());
}

}  // namespace dp::world
