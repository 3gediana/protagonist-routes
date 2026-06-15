#include "brain/SpatialMemoryBank.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

namespace neuro::routes::protagonist {

SpatialMemoryBank::SpatialMemoryBank(std::size_t grid_width, std::size_t grid_height, float world_width, float world_height)
    : w_(grid_width > 0 ? grid_width : 32),
      h_(grid_height > 0 ? grid_height : 32),
      world_w_(world_width > 0.0f ? world_width : 200.0f),
      world_h_(world_height > 0.0f ? world_height : 200.0f),
      cells_(w_ * h_) {
    reset();
}

void SpatialMemoryBank::reset() {
    for (std::size_t i = 0; i < cells_.size(); ++i) {
        cells_[i].pos = cellIndexToWorld(i);
        cells_[i].water_seen = 0.0f;
        cells_[i].food_seen = 0.0f;
        cells_[i].tree_seen = 0.0f;
        cells_[i].stone_seen = 0.0f;
        cells_[i].stick_seen = 0.0f;
        cells_[i].danger_seen = 0.0f;
        cells_[i].base_seen = 0.0f;
        cells_[i].fire_seen = 0.0f;
        cells_[i].worksite_seen = 0.0f;
        cells_[i].last_visit_age = 999.0f; // Very old
    }
}

void SpatialMemoryBank::tick(float dt) {
    for (auto& cell : cells_) {
        // Continuous temporal decay of visual traces
        cell.water_seen = std::max(0.0f, cell.water_seen - 0.001f * dt);
        cell.food_seen = std::max(0.0f, cell.food_seen - 0.005f * dt); // Food decays faster
        cell.tree_seen = std::max(0.0f, cell.tree_seen - 0.0001f * dt); // Trees are very stable
        cell.stone_seen = std::max(0.0f, cell.stone_seen - 0.001f * dt);
        cell.stick_seen = std::max(0.0f, cell.stick_seen - 0.003f * dt);
        cell.danger_seen = std::max(0.0f, cell.danger_seen - 0.01f * dt); // Danger (predators) decays fast
        cell.base_seen = std::max(0.0f, cell.base_seen - 0.00005f * dt); // Camps are extremely stable
        cell.fire_seen = std::max(0.0f, cell.fire_seen - 0.0005f * dt);
        cell.worksite_seen = std::max(0.0f, cell.worksite_seen - 0.0005f * dt);
        cell.last_visit_age += dt;
    }
}

void SpatialMemoryBank::recordSighting(Vec2 world_pos,
                                        float water,
                                        float food,
                                        float tree,
                                        float stone,
                                        float stick,
                                        float danger,
                                        float base,
                                        float fire,
                                        float worksite) {
    const std::size_t idx = worldToCellIndex(world_pos);
    if (idx >= cells_.size()) {
        return;
    }
    auto& cell = cells_[idx];
    if (water > 0.0f)  cell.water_seen = std::clamp(cell.water_seen + water, 0.0f, 1.0f);
    if (food > 0.0f)   cell.food_seen = std::clamp(cell.food_seen + food, 0.0f, 1.0f);
    if (tree > 0.0f)   cell.tree_seen = std::clamp(cell.tree_seen + tree, 0.0f, 1.0f);
    if (stone > 0.0f)  cell.stone_seen = std::clamp(cell.stone_seen + stone, 0.0f, 1.0f);
    if (stick > 0.0f)  cell.stick_seen = std::clamp(cell.stick_seen + stick, 0.0f, 1.0f);
    if (danger > 0.0f) cell.danger_seen = std::clamp(cell.danger_seen + danger, 0.0f, 1.0f);
    if (base > 0.0f)   cell.base_seen = std::clamp(cell.base_seen + base, 0.0f, 1.0f);
    if (fire > 0.0f)   cell.fire_seen = std::clamp(cell.fire_seen + fire, 0.0f, 1.0f);
    if (worksite > 0.0f) cell.worksite_seen = std::clamp(cell.worksite_seen + worksite, 0.0f, 1.0f);
    cell.last_visit_age = 0.0f; // Just visited/seen
}

namespace {

Vec2 findNearestCellMatching(Vec2 from_pos, const std::vector<SpatialMemoryCell>& cells, float threshold,
                             std::function<float(const SpatialMemoryCell&)> getter) {
    float min_dist_sq = std::numeric_limits<float>::max();
    Vec2 best_pos{0.0f, 0.0f};
    bool found = false;

    for (const auto& cell : cells) {
        if (getter(cell) >= threshold) {
            const float dx = cell.pos.x - from_pos.x;
            const float dy = cell.pos.y - from_pos.y;
            const float d2 = dx * dx + dy * dy;
            if (d2 < min_dist_sq) {
                min_dist_sq = d2;
                best_pos = cell.pos;
                found = true;
            }
        }
    }
    return found ? best_pos : Vec2{0.0f, 0.0f};
}

}  // namespace

Vec2 SpatialMemoryBank::findNearestWater(Vec2 from_pos, float threshold) const {
    return findNearestCellMatching(from_pos, cells_, threshold, [](const auto& c) { return c.water_seen; });
}

Vec2 SpatialMemoryBank::findNearestFood(Vec2 from_pos, float threshold) const {
    return findNearestCellMatching(from_pos, cells_, threshold, [](const auto& c) { return c.food_seen; });
}

Vec2 SpatialMemoryBank::findNearestTree(Vec2 from_pos, float threshold) const {
    return findNearestCellMatching(from_pos, cells_, threshold, [](const auto& c) { return c.tree_seen; });
}

Vec2 SpatialMemoryBank::findNearestStone(Vec2 from_pos, float threshold) const {
    return findNearestCellMatching(from_pos, cells_, threshold, [](const auto& c) { return c.stone_seen; });
}

Vec2 SpatialMemoryBank::findNearestStick(Vec2 from_pos, float threshold) const {
    return findNearestCellMatching(from_pos, cells_, threshold, [](const auto& c) { return c.stick_seen; });
}

Vec2 SpatialMemoryBank::findNearestDanger(Vec2 from_pos, float threshold) const {
    return findNearestCellMatching(from_pos, cells_, threshold, [](const auto& c) { return c.danger_seen; });
}

Vec2 SpatialMemoryBank::findNearestBase(Vec2 from_pos, float threshold) const {
    return findNearestCellMatching(from_pos, cells_, threshold, [](const auto& c) { return c.base_seen; });
}

Vec2 SpatialMemoryBank::findNearestFire(Vec2 from_pos, float threshold) const {
    return findNearestCellMatching(from_pos, cells_, threshold, [](const auto& c) { return c.fire_seen; });
}

Vec2 SpatialMemoryBank::findNearestWorksite(Vec2 from_pos, float threshold) const {
    return findNearestCellMatching(from_pos, cells_, threshold, [](const auto& c) { return c.worksite_seen; });
}

std::size_t SpatialMemoryBank::worldToCellIndex(Vec2 world_pos) const {
    const float x = std::clamp(world_pos.x, 0.0f, world_w_ - 0.01f);
    const float y = std::clamp(world_pos.y, 0.0f, world_h_ - 0.01f);
    const std::size_t cx = static_cast<std::size_t>(std::floor((x / world_w_) * w_));
    const std::size_t cy = static_cast<std::size_t>(std::floor((y / world_h_) * h_));
    return cy * w_ + cx;
}

Vec2 SpatialMemoryBank::cellIndexToWorld(std::size_t idx) const {
    const std::size_t cx = idx % w_;
    const std::size_t cy = idx / w_;
    const float cell_w = world_w_ / w_;
    const float cell_h = world_h_ / h_;
    return Vec2{
        (static_cast<float>(cx) + 0.5f) * cell_w,
        (static_cast<float>(cy) + 0.5f) * cell_h
    };
}

}  // namespace neuro::routes::protagonist
