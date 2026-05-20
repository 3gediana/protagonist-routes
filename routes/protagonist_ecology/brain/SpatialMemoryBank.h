#pragma once

#include "core/types/Vec2.h"

#include <cstddef>
#include <vector>

namespace neuro::routes::protagonist {

struct SpatialMemoryCell {
    Vec2 pos{0.0f, 0.0f};
    float water_seen{0.0f};
    float food_seen{0.0f};
    float tree_seen{0.0f};
    float stone_seen{0.0f};
    float stick_seen{0.0f};
    float danger_seen{0.0f};
    float base_seen{0.0f};
    float fire_seen{0.0f};
    float worksite_seen{0.0f};
    float last_visit_age{0.0f};
};

class SpatialMemoryBank {
public:
    SpatialMemoryBank(std::size_t grid_width, std::size_t grid_height, float world_width, float world_height);

    void reset();
    void tick(float dt);

    // Write a point memory (e.g. agent saw something at world position)
    void recordSighting(Vec2 world_pos,
                        float water,
                        float food,
                        float tree,
                        float stone,
                        float stick,
                        float danger,
                        float base,
                        float fire,
                        float worksite);

    // Query nearest cell matching threshold
    Vec2 findNearestWater(Vec2 from_pos, float threshold = 0.1f) const;
    Vec2 findNearestFood(Vec2 from_pos, float threshold = 0.1f) const;
    Vec2 findNearestTree(Vec2 from_pos, float threshold = 0.1f) const;
    Vec2 findNearestStone(Vec2 from_pos, float threshold = 0.1f) const;
    Vec2 findNearestStick(Vec2 from_pos, float threshold = 0.1f) const;
    Vec2 findNearestDanger(Vec2 from_pos, float threshold = 0.1f) const;
    Vec2 findNearestBase(Vec2 from_pos, float threshold = 0.1f) const;
    Vec2 findNearestFire(Vec2 from_pos, float threshold = 0.1f) const;
    Vec2 findNearestWorksite(Vec2 from_pos, float threshold = 0.1f) const;

    // Direct cell grid access
    std::size_t gridWidth() const { return w_; }
    std::size_t gridHeight() const { return h_; }
    const std::vector<SpatialMemoryCell>& cells() const { return cells_; }

private:
    std::size_t worldToCellIndex(Vec2 world_pos) const;
    Vec2 cellIndexToWorld(std::size_t idx) const;

    std::size_t w_;
    std::size_t h_;
    float world_w_;
    float world_h_;
    std::vector<SpatialMemoryCell> cells_;
};

}  // namespace neuro::routes::protagonist
