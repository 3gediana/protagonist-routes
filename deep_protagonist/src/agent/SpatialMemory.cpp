#include "agent/SpatialMemory.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace dp::agent {

void SpatialMemory::configure(float origin_x, float origin_y) {
    origin_x_ = origin_x;
    origin_y_ = origin_y;
    clear();
}

void SpatialMemory::clear() {
    std::memset(cells_.data(), 0, sizeof(cells_));
}

bool SpatialMemory::world_to_grid_(float wx, float wy,
                                    int& gx, int& gy) const {
    float fx = (wx - origin_x_) / CELL_SIZE_M;
    float fy = (wy - origin_y_) / CELL_SIZE_M;
    gx = static_cast<int>(std::floor(fx));
    gy = static_cast<int>(std::floor(fy));
    return gx >= 0 && gx < GRID && gy >= 0 && gy < GRID;
}

void SpatialMemory::write(float wx, float wy,
                          bool saw_water,
                          bool saw_food,
                          bool saw_resource,
                          bool saw_danger,
                          bool saw_building) {
    int gx, gy;
    if (!world_to_grid_(wx, wy, gx, gy)) return;
    int base = (gy * GRID + gx) * FEATURES;
    cells_[base + CH_VISITED]  = 1.0f;
    if (saw_water)    cells_[base + CH_WATER]    = 1.0f;
    if (saw_food)     cells_[base + CH_FOOD]     = 1.0f;
    if (saw_resource) cells_[base + CH_RESOURCE] = 1.0f;
    if (saw_danger)   cells_[base + CH_DANGER]   = 1.0f;
    if (saw_building) cells_[base + CH_BUILDING] = 1.0f;
}

void SpatialMemory::read_window(float wx, float wy, float* out) const {
    int cx, cy;
    world_to_grid_(wx, wy, cx, cy);
    // half offset: WINDOW=8 -> centre is at offset 3..4. We use floor(WINDOW/2)
    // so the agent sits in cell index WINDOW/2 of the window.
    const int half = WINDOW / 2;
    int idx = 0;
    for (int wy_i = 0; wy_i < WINDOW; ++wy_i) {
        int gy = cy - half + wy_i;
        for (int wx_i = 0; wx_i < WINDOW; ++wx_i) {
            int gx = cx - half + wx_i;
            if (gx < 0 || gx >= GRID || gy < 0 || gy >= GRID) {
                for (int f = 0; f < FEATURES; ++f) out[idx++] = 0.0f;
            } else {
                int base = (gy * GRID + gx) * FEATURES;
                for (int f = 0; f < FEATURES; ++f) {
                    out[idx++] = cells_[base + f];
                }
            }
        }
    }
}

int SpatialMemory::visited_count() const {
    int n = 0;
    for (int i = 0; i < CELLS; ++i) {
        if (cells_[i * FEATURES + CH_VISITED] > 0.5f) ++n;
    }
    return n;
}

float SpatialMemory::feature_avg(Channel c) const {
    double sum = 0.0;
    for (int i = 0; i < CELLS; ++i) sum += cells_[i * FEATURES + c];
    return static_cast<float>(sum / CELLS);
}

}  // namespace dp::agent
