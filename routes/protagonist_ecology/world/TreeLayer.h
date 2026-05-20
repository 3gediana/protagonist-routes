#pragma once

#include "core/interfaces/IWorldLayer.h"
#include "core/types/Vec2.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <vector>

namespace neuro {
class IWorld;
class ObjectLayer;
}

namespace neuro::routes::protagonist {

class BaseLayer;

struct TreeResource {
    std::size_t id;
    Vec2 position;
    float integrity;
    float max_integrity;
    float trunk_radius;
    std::size_t wood_yield;
    bool available;
    std::size_t regrow_ticks_remaining;
};

class TreeLayer final : public IWorldLayer {
public:
    static constexpr const char* kName = "protagonist_trees";

    TreeLayer(std::size_t tree_count,
              float spawn_radius_min,
              float spawn_radius_max,
              float trunk_radius,
              float max_integrity,
              std::size_t wood_yield,
              std::size_t regrow_ticks);

    const char* layerName() const override { return kName; }
    void onAttach(IWorld& world) override;
    void tick(IWorld& world, float dt) override;
    int tickOrder() const override { return 145; }

    const std::vector<TreeResource>& trees() const { return trees_; }
    std::optional<TreeResource> nearestTree(Vec2 position) const;
    bool inChopRange(Vec2 position, float chop_radius) const;
    bool chopNearest(Vec2 position, float chop_radius, float damage, IWorld& world);
    std::size_t availableTreeCount() const;
    std::size_t harvestedCount() const { return harvested_count_; }

private:
    void seedTrees(Vec2 world_size);

    BaseLayer* base_ = nullptr;
    ObjectLayer* objects_ = nullptr;
    std::size_t tree_count_;
    float spawn_radius_min_;
    float spawn_radius_max_;
    float trunk_radius_;
    float max_integrity_;
    std::size_t wood_yield_;
    std::size_t regrow_ticks_;
    std::mt19937 rng_;
    std::vector<TreeResource> trees_;
    std::size_t harvested_count_ = 0;
};

}  // namespace neuro::routes::protagonist
