#pragma once

#include "agent/CapsuleAgent.hpp"
#include "agent/Shelter.hpp"
#include "render/FlyCamera.hpp"
#include "world/AnimalSystem.hpp"
#include "world/BuildingSystem.hpp"
#include "world/Decorations.hpp"
#include "world/FarmingSystem.hpp"
#include "world/PlantSystem.hpp"
#include "world/ResourceSystem.hpp"
#include "world/World.hpp"

#include <raylib.h>

#include <vector>

namespace dp::render {

// Renders the full world (terrain + biomes + decorations + caves + agent +
// plants + animals) using raylib. Owns its GPU resources.
class WorldRenderer {
public:
    WorldRenderer();
    ~WorldRenderer();
    WorldRenderer(const WorldRenderer&) = delete;
    WorldRenderer& operator=(const WorldRenderer&) = delete;

    void build(const dp::world::World& world,
               const dp::world::Decorations& decorations);

    // `tint` is multiplied onto every static-world draw call so day/night
    // shading affects everything.
    void draw(const FlyCamera& cam,
              const dp::agent::CapsuleAgent& agent,
              const dp::world::World& world,
              const dp::world::Decorations& decorations,
              const dp::world::PlantSystem& plants,
              const dp::world::AnimalSystem& animals,
              const dp::world::ResourceSystem& resources,
              const dp::agent::Shelter& shelter,
              const dp::world::BuildingSystem& buildings,
              const dp::world::FarmingSystem& farming,
              dp::agent::ItemKind worn_clothing,
              Color tint = WHITE);

private:
    bool built_ = false;

    // Terrain mesh
    Mesh   terrain_mesh_{};
    Model  terrain_model_{};

    // One small cuboid model per cave, drawn semi-transparently to make the
    // cave volume visible from outside.
    std::vector<BoundingBox> cave_boxes_;

    // S3.10 polish B: optional GLB models for the four building types.
    // Loaded from DP_ASSET_DIR (Quaternius Medieval_Village_Pack). If a
    // model fails to load (meshCount == 0) we fall back to the original
    // geometry boxes.
    Model  building_models_[4]{};
    bool   building_models_loaded_[4] = { false, false, false, false };
    void   load_building_models_();

    void build_terrain_mesh(const dp::world::Heightmap& hm);
};

}  // namespace dp::render
