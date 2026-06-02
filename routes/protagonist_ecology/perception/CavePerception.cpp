#include "perception/CavePerception.h"

#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/terrain/TerrainLayer.h"

#include <algorithm>
#include <limits>

namespace neuro::routes::protagonist {

void CavePerception::sense(const IAgent& self, const IWorld& world, std::span<float> out) const {
    if (out.size() < kDim) {
        return;
    }
    std::fill(out.begin(), out.end(), 0.0f);
    out[3] = 1.0f;  // default: no cave known within sense range

    const auto* terrain = world.getLayer<TerrainLayer>();
    if (terrain == nullptr) {
        return;
    }

    const Vec2 pos = self.position();
    if (terrain->inCave(pos.x, pos.y)) {
        out[0] = 1.0f;
        out[3] = 0.0f;  // already sheltered
        return;
    }

    // Cache cave-cell centers once (caves are static after world-gen).
    if (!cache_built_) {
        const auto& mask = terrain->caveMask();
        const float cs = mask.cellSize();
        for (std::size_t gy = 0; gy < mask.height(); ++gy) {
            for (std::size_t gx = 0; gx < mask.width(); ++gx) {
                if (mask.at(gx, gy)) {
                    cave_cells_.push_back(Vec2{(static_cast<float>(gx) + 0.5f) * cs,
                                               (static_cast<float>(gy) + 0.5f) * cs});
                }
            }
        }
        cache_built_ = true;
    }
    if (cave_cells_.empty()) {
        return;
    }

    const Vec2* nearest = nullptr;
    float best = std::numeric_limits<float>::max();
    for (const auto& cell : cave_cells_) {
        const float d2 = Vec2::distanceSquared(cell, pos);
        if (d2 < best) {
            best = d2;
            nearest = &cell;
        }
    }
    if (nearest == nullptr) {
        return;
    }

    const Vec2 delta = *nearest - pos;
    const float dist = delta.length();
    if (dist > 0.0001f) {
        out[1] = delta.x / dist;
        out[2] = delta.y / dist;
    }
    const float r = sense_radius_ > 0.0f ? sense_radius_ : 1.0f;
    out[3] = std::clamp(dist / r, 0.0f, 1.0f);
}

}  // namespace neuro::routes::protagonist
