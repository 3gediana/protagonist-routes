#include "core/world/layers/terrain/TerrainLayer.h"

#include <algorithm>
#include <cmath>

namespace neuro {

TerrainLayer::TerrainLayer(const Vec2& world_size, const TerrainParams& params)
    : params_(params),
      height_field_(params.grid_resolution, params.grid_resolution,
                    std::max(world_size.x, world_size.y) / static_cast<float>(params.grid_resolution)),
      cave_mask_(params.grid_resolution, params.grid_resolution,
                 std::max(world_size.x, world_size.y) / static_cast<float>(params.grid_resolution)) {
    TerrainGenerator::generateHeightField(height_field_, params_, params_.seed);
    TerrainGenerator::generateCaveMask(cave_mask_, params_, params_.seed);
}

void TerrainLayer::onAttach(IWorld& world) {
    (void)world;
}

void TerrainLayer::tick(IWorld& world, float dt) {
    (void)world;
    (void)dt;
}

float TerrainLayer::heightAt(float x, float y) const {
    return height_field_.heightAt(x, y);
}

Vec2 TerrainLayer::slopeAt(float x, float y) const {
    return Vec2{height_field_.slopeX(x, y), height_field_.slopeY(x, y)};
}

bool TerrainLayer::inCave(float x, float y) const {
    return cave_mask_.isCave(x, y);
}

bool TerrainLayer::isVisible(Vec2 from, Vec2 to) const {
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float dist = std::sqrt(dx * dx + dy * dy);
    if (dist < 1.0f) return true;

    const int steps = static_cast<int>(dist / (height_field_.cellSize() * 0.5f));
    if (steps <= 1) return true;

    const float step_x = dx / static_cast<float>(steps);
    const float step_y = dy / static_cast<float>(steps);

    float cx = from.x;
    float cy = from.y;
    const float from_height = heightAt(from.x, from.y);

    for (int i = 1; i < steps; ++i) {
        cx += step_x;
        cy += step_y;
        const float terrain_h = heightAt(cx, cy);

        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const float ray_h = from_height * (1.0f - t) + heightAt(to.x, to.y) * t;

        if (terrain_h > ray_h + 0.5f) {
            return false;
        }
    }

    return true;
}

}  // namespace neuro
