#include "world/RiverSystem.hpp"

#include <algorithm>
#include <cmath>
#include <random>

namespace dp::world {

namespace {

// Look at the 8 neighbours and return the (di, dj) offset of the lowest one.
// If all neighbours are higher, returns (0, 0).
void steepest_descent(const Heightmap& hm, int i, int j, int& di, int& dj) {
    di = 0; dj = 0;
    float h_here = hm.at(i, j);
    float h_min  = h_here;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            float h = hm.at(i + dx, j + dy);
            if (h < h_min) {
                h_min = h;
                di = dx; dj = dy;
            }
        }
    }
}

}  // namespace

void RiverSystem::generate(uint64_t seed, int count,
                           Heightmap& hm, BiomeMap& bm) {
    segments_.clear();
    std::mt19937_64 rng(seed);
    const int nx = hm.size_x();
    const int ny = hm.size_y();
    const float scale = hm.cell_scale();

    // Find the global terrain max once
    float h_max = 1e-6f;
    for (float h : hm.raw()) {
        if (h > h_max) h_max = h;
    }

    for (int r = 0; r < count; ++r) {
        // Pick a random starting cell whose height is in the upper third.
        int start_i = -1, start_j = -1;
        for (int attempts = 0; attempts < 200 && start_i < 0; ++attempts) {
            int i = std::uniform_int_distribution<int>(2, nx - 3)(rng);
            int j = std::uniform_int_distribution<int>(2, ny - 3)(rng);
            if (hm.at(i, j) >= h_max * 0.6f) {
                start_i = i; start_j = j;
            }
        }
        if (start_i < 0) continue;

        // Walk down. At each step, pick the lowest neighbour, carve, mark.
        int i = start_i, j = start_j;
        const int max_steps = nx * 2;
        int prev_i = i, prev_j = j;

        for (int step = 0; step < max_steps; ++step) {
            // Carve: flatten and lower this cell, mark as river
            float carved = std::max(0.0f, hm.at(i, j) - 0.5f);
            hm.set(i, j, carved);
            bm.mark_river(i, j);

            // Mark immediate neighbours as swamp (within +/-1)
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    bm.mark_swamp(i + dx, j + dy);
                }
            }

            if (step > 0) {
                segments_.push_back({
                    prev_i * scale, prev_j * scale,
                    i      * scale, j      * scale
                });
            }
            prev_i = i; prev_j = j;

            int di, dj;
            steepest_descent(hm, i, j, di, dj);
            if (di == 0 && dj == 0) break;  // local minimum, river ends

            i += di;
            j += dj;
            if (i <= 1 || i >= nx - 2 || j <= 1 || j >= ny - 2) break;
        }
    }
}

}  // namespace dp::world
