#pragma once

#include "core/interfaces/IWorldLayer.h"
#include "core/world/layers/terrain/HeightField.h"
#include "core/world/layers/terrain/CaveMask.h"
#include "core/world/layers/terrain/TerrainGenerator.h"
#include "core/types/Vec2.h"

namespace neuro {

class TerrainLayer final : public IWorldLayer {
public:
    static constexpr const char* kName = "terrain";

    TerrainLayer(const Vec2& world_size, const TerrainParams& params);

    const char* layerName() const override { return kName; }
    void onAttach(IWorld& world) override;
    void tick(IWorld& world, float dt) override;

    float heightAt(float x, float y) const;
    Vec2 slopeAt(float x, float y) const;
    bool inCave(float x, float y) const;
    bool isVisible(Vec2 from, Vec2 to) const;

    float slopeResistance() const { return params_.slope_resistance; }
    float caveShelterFactor() const { return params_.cave_shelter_factor; }
    float minVelocityFactor() const { return params_.min_velocity_factor; }
    float heightScale() const { return params_.height_scale; }

    const HeightField& heightField() const { return height_field_; }
    const CaveMask& caveMask() const { return cave_mask_; }

private:
    TerrainParams params_;
    HeightField height_field_;
    CaveMask cave_mask_;
};

}  // namespace neuro
