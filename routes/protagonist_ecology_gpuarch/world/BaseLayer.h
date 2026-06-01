#pragma once

#include "core/interfaces/IWorldLayer.h"
#include "core/types/Vec2.h"

namespace neuro::routes::protagonist {

class BaseLayer final : public IWorldLayer {
public:
    static constexpr const char* kName = "protagonist_base";

    BaseLayer(Vec2 center,
              float base_radius,
              float stockpile_radius,
              float nest_radius,
              float stockpile_offset,
              float nest_offset);

    const char* layerName() const override { return kName; }
    void onAttach(IWorld& world) override;
    // No per-tick logic required: shelter physics is provided by completed
    // worksites (WorksiteLayer::enforcePredatorObstacles), not by a magical
    // damage rule on nest entry.
    void tick(IWorld& /*world*/, float /*dt*/) override {}

    Vec2 baseCenter() const { return base_center_; }
    Vec2 stockpileCenter() const { return stockpile_center_; }
    Vec2 nestCenter() const { return nest_center_; }

    float baseRadius() const { return base_radius_; }
    float stockpileRadius() const { return stockpile_radius_; }
    float nestRadius() const { return nest_radius_; }

    bool inBase(Vec2 pos) const;
    bool inStockpile(Vec2 pos) const;
    bool inNest(Vec2 pos) const;

private:
    Vec2 clampToWorld(Vec2 p, Vec2 world_size) const;

    Vec2 base_center_;
    Vec2 stockpile_center_;
    Vec2 nest_center_;
    float base_radius_;
    float stockpile_radius_;
    float nest_radius_;
    float stockpile_offset_;
    float nest_offset_;
};

}  // namespace neuro::routes::protagonist
