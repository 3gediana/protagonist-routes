#include "world/CaveSystem.hpp"

#include <random>

namespace dp::world {

void CaveSystem::generate_random(uint64_t seed, int count,
                                 float world_size_x, float world_size_y) {
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<float> px(world_size_x * 0.15f, world_size_x * 0.85f);
    std::uniform_real_distribution<float> py(world_size_y * 0.15f, world_size_y * 0.85f);
    std::uniform_real_distribution<float> hx(8.0f, 16.0f);   // half-extents
    std::uniform_real_distribution<float> hy(8.0f, 16.0f);
    std::uniform_real_distribution<float> dz(8.0f, 14.0f);   // ceiling height
    std::uniform_real_distribution<float> fz(2.0f, 6.0f);    // floor depth offset

    caves_.clear();
    caves_.reserve(count);
    for (int i = 0; i < count; ++i) {
        CaveBox c;
        c.center_x  = px(rng);
        c.center_y  = py(rng);
        c.half_x    = hx(rng);
        c.half_y    = hy(rng);
        c.floor_z   = -fz(rng);     // floor is a few meters below sea level
        c.ceiling_z = dz(rng);      // ceiling is a few meters above sea level
        // The cave volume spans roughly z = [-2..-6, 8..14] which means it
        // typically pokes out of low-lying terrain (entrance) but stays
        // below high terrain (true underground).
        caves_.push_back(c);
    }
}

}  // namespace dp::world
