#include "world/Decorations.hpp"

#include <cmath>

namespace dp::world {

float Decorations::frand() {
    rng_state_ ^= rng_state_ >> 12;
    rng_state_ ^= rng_state_ << 25;
    rng_state_ ^= rng_state_ >> 27;
    uint64_t r = rng_state_ * 0x2545F4914F6CDD1DULL;
    return static_cast<float>(r & 0xFFFFFFu) / 16777216.0f;
}

void Decorations::generate(uint64_t seed, const World& world) {
    rng_state_ = seed ? seed : 1;
    items_.clear();

    const auto& hm = world.heightmap();
    const auto& bm = world.biomes();
    const int nx = hm.size_x();
    const int ny = hm.size_y();
    const float scale = hm.cell_scale();

    // Stride: only attempt placement every N cells; helps cap total count.
    auto try_place = [&](int i, int j, DecorationKind k, float prob,
                         float scale_min, float scale_max) {
        if (frand() > prob) return;
        // Place somewhere inside the cell (small jitter)
        float wx = (i + frand() * 0.9f + 0.05f) * scale;
        float wy = (j + frand() * 0.9f + 0.05f) * scale;
        float wz = world.surface_height(wx, wy);
        // Reject if the spot is underground (cave) or the slope is too steep
        if (world.is_underground(wx, wy, wz + 0.5f)) return;
        Decoration d;
        d.x = wx; d.y = wy; d.z = wz;
        d.scale = scale_min + frand() * (scale_max - scale_min);
        d.rotation = frand() * 6.2831853f;
        d.kind = k;
        items_.push_back(d);
    };

    for (int j = 0; j < ny; j += 2) {
        for (int i = 0; i < nx; i += 2) {
            Biome b = bm.at(i, j);
            switch (b) {
                case Biome::Forest:
                    try_place(i, j, DecorationKind::Tree,   0.55f, 1.6f, 3.0f);
                    break;
                case Biome::Plain:
                    // Sparse trees on plains too, gives variety
                    try_place(i, j, DecorationKind::Tree,   0.05f, 1.0f, 1.8f);
                    break;
                case Biome::Mountain:
                    try_place(i, j, DecorationKind::Rock,   0.30f, 0.6f, 1.6f);
                    break;
                case Biome::Desert:
                    try_place(i, j, DecorationKind::Cactus, 0.10f, 0.7f, 1.5f);
                    break;
                case Biome::Swamp:
                    try_place(i, j, DecorationKind::Tree,   0.10f, 0.8f, 1.4f);
                    break;
                default:
                    break;
            }
        }
    }
}

}  // namespace dp::world
