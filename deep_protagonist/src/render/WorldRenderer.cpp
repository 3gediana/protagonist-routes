#include "render/WorldRenderer.hpp"

#include <raymath.h>
#include <rlgl.h>

#include <cmath>
#include <cstring>
#include <string>

namespace dp::render {

WorldRenderer::WorldRenderer() = default;

WorldRenderer::~WorldRenderer() {
    if (built_) {
        UnloadModel(terrain_model_);  // also unloads its mesh
    }
    for (int i = 0; i < 4; ++i) {
        if (building_models_loaded_[i]) {
            UnloadModel(building_models_[i]);
        }
    }
}

namespace {
// 4 candidate Quaternius GLB files - one per BuildingType. Chosen from
// Medieval_Village_Pack at varying file sizes (small to large) so they
// visually map to "small cottage -> wood house -> stone house -> big".
// If raylib can't load one, we drop back to the colored geometry box.
constexpr const char* kBuildingModelRel[4] = {
    "/quaternius/Medieval_Village_Pack/alxTTFjKDM_alxTTFjKDM.glb",  // Shed
    "/quaternius/Medieval_Village_Pack/he3p42mUTH_he3p42mUTH.glb",  // WoodHouse
    "/quaternius/Medieval_Village_Pack/qhNQSOGGbi_qhNQSOGGbi.glb",  // StoneHouse
    "/quaternius/Medieval_Village_Pack/ux44tbeQvj_ux44tbeQvj.glb",  // BigHouse
};
}  // namespace

void WorldRenderer::load_building_models_() {
#ifdef DP_ASSET_DIR
    for (int i = 0; i < 4; ++i) {
        std::string path = std::string(DP_ASSET_DIR) + kBuildingModelRel[i];
        Model m = LoadModel(path.c_str());
        // raylib marks a failed load with meshCount == 0
        if (m.meshCount > 0) {
            building_models_[i] = m;
            building_models_loaded_[i] = true;
        } else {
            // Free any partial state and stay in fallback mode.
            UnloadModel(m);
            building_models_loaded_[i] = false;
        }
    }
#endif
}

// ---------------------------------------------------------------------------
// Build a terrain mesh from a Heightmap + BiomeMap. Each vertex's color is
// looked up from its biome class (rather than a fixed elevation gradient),
// giving plains/forest/mountain/desert/swamp/river their own visible feel.
// ---------------------------------------------------------------------------
namespace {
struct BiomeColor { unsigned char r, g, b; };
constexpr BiomeColor biome_color(dp::world::Biome b) {
    switch (b) {
        case dp::world::Biome::Plain:    return {110, 180,  80};
        case dp::world::Biome::Forest:   return { 30, 110,  40};
        case dp::world::Biome::Mountain: return {150, 145, 140};
        case dp::world::Biome::River:    return { 60, 130, 220};
        case dp::world::Biome::Swamp:    return { 80,  90,  60};
        case dp::world::Biome::Desert:   return {220, 195, 130};
        default:                          return {200, 200, 200};
    }
}
}  // namespace

void WorldRenderer::build_terrain_mesh(const dp::world::Heightmap& hm) {
    // Note: we build a mesh from heightmap alone here so we can keep the API
    // small. Per-vertex biome colour is filled in by build() after we have
    // access to the BiomeMap.
    const int nx = hm.size_x();
    const int ny = hm.size_y();
    const float scale = hm.cell_scale();
    const int n_verts = nx * ny;
    const int n_quads = (nx - 1) * (ny - 1);
    const int n_tris  = n_quads * 2;

    Mesh m{};
    m.vertexCount   = n_verts;
    m.triangleCount = n_tris;
    m.vertices  = (float*)         MemAlloc(sizeof(float) * 3 * n_verts);
    m.normals   = (float*)         MemAlloc(sizeof(float) * 3 * n_verts);
    m.colors    = (unsigned char*) MemAlloc(sizeof(unsigned char) * 4 * n_verts);
    m.indices   = (unsigned short*) MemAlloc(sizeof(unsigned short) * 3 * n_tris);

    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            int v = i + j * nx;
            float wx = i * scale;
            float wy = j * scale;
            float wz = hm.at(i, j);
            m.vertices[v * 3 + 0] = wx;
            m.vertices[v * 3 + 1] = wy;
            m.vertices[v * 3 + 2] = wz;
            // Default colour - will be overridden per-biome below in build()
            m.colors[v * 4 + 0] = 130;
            m.colors[v * 4 + 1] = 170;
            m.colors[v * 4 + 2] = 100;
            m.colors[v * 4 + 3] = 255;
        }
    }

    // Triangle indices (two tris per quad)
    int t = 0;
    for (int j = 0; j < ny - 1; ++j) {
        for (int i = 0; i < nx - 1; ++i) {
            unsigned short v00 = (unsigned short)(i     + j * nx);
            unsigned short v10 = (unsigned short)(i + 1 + j * nx);
            unsigned short v01 = (unsigned short)(i     + (j + 1) * nx);
            unsigned short v11 = (unsigned short)(i + 1 + (j + 1) * nx);
            m.indices[t++] = v00; m.indices[t++] = v10; m.indices[t++] = v11;
            m.indices[t++] = v00; m.indices[t++] = v11; m.indices[t++] = v01;
        }
    }

    // Compute per-vertex normals from heightmap gradient
    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            float dx, dy;
            hm.sample_gradient(i * scale, j * scale, dx, dy);
            // Normal of surface z = h(x,y) is (-dh/dx, -dh/dy, 1) normalized
            Vector3 n = Vector3{ -dx, -dy, 1.0f };
            n = Vector3Normalize(n);
            int v = i + j * nx;
            m.normals[v * 3 + 0] = n.x;
            m.normals[v * 3 + 1] = n.y;
            m.normals[v * 3 + 2] = n.z;
        }
    }

    UploadMesh(&m, false);
    terrain_mesh_ = m;
    terrain_model_ = LoadModelFromMesh(m);
}

void WorldRenderer::build(const dp::world::World& world,
                          const dp::world::Decorations& /*decorations*/) {
    build_terrain_mesh(world.heightmap());

    // Re-colour vertices using per-cell biome classification.
    const auto& hm = world.heightmap();
    const auto& bm = world.biomes();
    const int nx = hm.size_x();
    const int ny = hm.size_y();
    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            int v = i + j * nx;
            BiomeColor c = biome_color(bm.at(i, j));
            terrain_mesh_.colors[v * 4 + 0] = c.r;
            terrain_mesh_.colors[v * 4 + 1] = c.g;
            terrain_mesh_.colors[v * 4 + 2] = c.b;
            terrain_mesh_.colors[v * 4 + 3] = 255;
        }
    }
    UpdateMeshBuffer(terrain_mesh_, 3,  // attribute index 3 = colours
                     terrain_mesh_.colors,
                     sizeof(unsigned char) * 4 * terrain_mesh_.vertexCount, 0);

    cave_boxes_.clear();
    cave_boxes_.reserve(world.caves().caves().size());
    for (const auto& c : world.caves().caves()) {
        BoundingBox bb;
        bb.min = Vector3{ c.center_x - c.half_x, c.center_y - c.half_y, c.floor_z };
        bb.max = Vector3{ c.center_x + c.half_x, c.center_y + c.half_y, c.ceiling_z };
        cave_boxes_.push_back(bb);
    }

    // Load GLB building models if available. Failures stay silent and
    // we keep using the geometry fallback for the missing slots.
    load_building_models_();

    built_ = true;
}

namespace {
inline Color tint_mul(Color base, Color tint) {
    return Color{
        static_cast<unsigned char>((base.r * tint.r) / 255),
        static_cast<unsigned char>((base.g * tint.g) / 255),
        static_cast<unsigned char>((base.b * tint.b) / 255),
        base.a
    };
}
}  // namespace

void WorldRenderer::draw(const FlyCamera& cam,
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
                         Color tint) {
    if (!built_) return;

    BeginMode3D(cam.raw());

    // Terrain
    DrawModel(terrain_model_, Vector3{ 0, 0, 0 }, 1.0f, tint);

    // ---- Decorations: only draw items inside the camera's view radius ----
    // Trees / rocks / cacti as cheap procedural geometry (cylinders+cones).
    constexpr float kDrawRadius = 200.0f;
    constexpr float kDrawRadius2 = kDrawRadius * kDrawRadius;
    Vector3 cam_pos = cam.raw().position;
    for (const auto& d : decorations.items()) {
        float dx = d.x - cam_pos.x;
        float dy = d.y - cam_pos.y;
        if (dx * dx + dy * dy > kDrawRadius2) continue;
        switch (d.kind) {
            case dp::world::DecorationKind::Tree: {
                float h_trunk = 1.6f * d.scale;
                float r_trunk = 0.18f * d.scale;
                float h_cano  = 3.0f * d.scale;
                float r_cano  = 1.2f * d.scale;
                Vector3 base = Vector3{ d.x, d.y, d.z };
                Vector3 top  = Vector3{ d.x, d.y, d.z + h_trunk };
                DrawCylinderEx(base, top, r_trunk, r_trunk, 8,
                               tint_mul(Color{ 90, 60, 30, 255 }, tint));
                Vector3 cano_base = Vector3{ d.x, d.y, d.z + h_trunk };
                Vector3 cano_top  = Vector3{ d.x, d.y, d.z + h_trunk + h_cano };
                DrawCylinderEx(cano_base, cano_top, r_cano, 0.0f, 8,
                               tint_mul(Color{ 30, 110, 40, 255 }, tint));
                break;
            }
            case dp::world::DecorationKind::Rock: {
                Vector3 size = Vector3{ 1.0f * d.scale, 1.0f * d.scale, 0.8f * d.scale };
                Vector3 center = Vector3{ d.x, d.y, d.z + size.z * 0.5f };
                DrawCubeV(center, size,
                          tint_mul(Color{ 120, 115, 110, 255 }, tint));
                break;
            }
            case dp::world::DecorationKind::Cactus: {
                float h = 2.0f * d.scale;
                float r = 0.15f * d.scale;
                Vector3 base = Vector3{ d.x, d.y, d.z };
                Vector3 top  = Vector3{ d.x, d.y, d.z + h };
                Color c = tint_mul(Color{ 60, 130, 60, 255 }, tint);
                DrawCylinderEx(base, top, r, r, 6, c);
                Vector3 a1_base = Vector3{ d.x, d.y, d.z + h * 0.6f };
                Vector3 a1_top  = Vector3{ d.x + 0.5f * d.scale, d.y, d.z + h * 0.9f };
                DrawCylinderEx(a1_base, a1_top, r * 0.7f, r * 0.7f, 6, c);
                Vector3 a2_base = Vector3{ d.x, d.y, d.z + h * 0.5f };
                Vector3 a2_top  = Vector3{ d.x - 0.4f * d.scale, d.y, d.z + h * 0.85f };
                DrawCylinderEx(a2_base, a2_top, r * 0.7f, r * 0.7f, 6, c);
                break;
            }
        }
    }

    // Cave volumes - semi-transparent boxes so the volume is visible
    for (const auto& bb : cave_boxes_) {
        Vector3 size  = Vector3Subtract(bb.max, bb.min);
        Vector3 center = Vector3Scale(Vector3Add(bb.min, bb.max), 0.5f);
        Color cave_col = Color{ 200, 80, 220, 90 };  // pinkish, translucent
        DrawCubeV(center, size, cave_col);
        DrawCubeWiresV(center, size, Color{ 220, 100, 240, 200 });
    }

    // Plants - color-coded by kind, only show ripe ones
    for (const auto& p : plants.plants()) {
        if (!p.ripe) continue;
        Color col;
        float r = 0.3f;
        switch (p.kind) {
            case dp::world::PlantKind::Berry:    col = Color{220, 50, 90, 255}; r = 0.20f; break;
            case dp::world::PlantKind::Fruit:    col = Color{255,170, 30, 255}; r = 0.30f; break;
            case dp::world::PlantKind::Cactus:   col = Color{100,220,150, 255}; r = 0.25f; break;
            case dp::world::PlantKind::Mushroom: col = Color{210,210, 90, 255}; r = 0.22f; break;
        }
        DrawSphere(Vector3{ p.x, p.y, p.z }, r, col);
    }

    // Animals - cheap stylized shapes; one cube + ear/cone marks per kind
    for (const auto& a : animals.animals()) {
        if (a.state == dp::world::AnimalState::Dead) continue;
        Vector3 pos = Vector3{ a.pos_x, a.pos_y, a.pos_z };
        switch (a.kind) {
            case dp::world::AnimalKind::Rabbit: {
                // Small white cube + two pointy ears
                DrawCube(pos, 0.35f, 0.35f, 0.30f, Color{240, 240, 240, 255});
                Vector3 ear = Vector3{ pos.x, pos.y, pos.z + 0.35f };
                DrawCube(ear, 0.10f, 0.10f, 0.30f, Color{220, 200, 200, 255});
                break;
            }
            case dp::world::AnimalKind::Deer: {
                Vector3 body_pos = Vector3{ pos.x, pos.y, pos.z + 0.4f };
                DrawCube(body_pos, 1.0f, 0.5f, 0.7f, Color{160, 110, 60, 255});
                Vector3 head = Vector3{ pos.x + std::cos(a.yaw)*0.55f,
                                        pos.y + std::sin(a.yaw)*0.55f,
                                        pos.z + 0.9f };
                DrawCube(head, 0.4f, 0.4f, 0.4f, Color{180, 130, 70, 255});
                // antlers
                DrawCube(Vector3{head.x, head.y, head.z + 0.35f}, 0.08f, 0.08f, 0.3f,
                         Color{120, 80, 40, 255});
                break;
            }
            case dp::world::AnimalKind::Wolf: {
                Vector3 body_pos = Vector3{ pos.x, pos.y, pos.z + 0.3f };
                Color body_col = (a.state == dp::world::AnimalState::Hunt)
                    ? Color{220, 60, 60, 255}    // angry red when hunting
                    : Color{ 90, 90,100, 255};   // grey otherwise
                DrawCube(body_pos, 1.1f, 0.4f, 0.55f, body_col);
                Vector3 head = Vector3{ pos.x + std::cos(a.yaw)*0.6f,
                                        pos.y + std::sin(a.yaw)*0.6f,
                                        pos.z + 0.65f };
                DrawCube(head, 0.35f, 0.35f, 0.35f, body_col);
                break;
            }
            case dp::world::AnimalKind::Fish: {
                DrawCube(pos, 0.4f, 0.18f, 0.18f, Color{120, 180, 220, 255});
                break;
            }
            case dp::world::AnimalKind::Eagle: {
                // Spinning cross to suggest wings
                DrawCube(pos, 1.6f, 0.2f, 0.05f, Color{ 60, 50, 40, 255});
                DrawCube(pos, 0.3f, 0.3f, 0.15f, Color{ 80, 70, 50, 255});
                break;
            }
        }
    }

    // Resources: only available nodes. Same LOD radius as decorations.
    constexpr float kResRadius2 = 200.0f * 200.0f;
    Vector3 cam_pos2 = cam.raw().position;
    for (const auto& r : resources.resources()) {
        if (!r.available) continue;
        float dx = r.x - cam_pos2.x, dy = r.y - cam_pos2.y;
        if (dx*dx + dy*dy > kResRadius2) continue;
        switch (r.kind) {
            case dp::world::ResourceKind::Wood: {
                // brown stick lying down
                Vector3 a1 = Vector3{ r.x - 0.4f, r.y, r.z + 0.05f };
                Vector3 a2 = Vector3{ r.x + 0.4f, r.y, r.z + 0.05f };
                DrawCylinderEx(a1, a2, 0.06f, 0.04f, 6,
                               tint_mul(Color{120, 80, 35, 255}, tint));
                break;
            }
            case dp::world::ResourceKind::Stone: {
                Vector3 c = Vector3{ r.x, r.y, r.z + 0.18f };
                DrawCubeV(c, Vector3{0.45f, 0.45f, 0.30f},
                          tint_mul(Color{160, 160, 160, 255}, tint));
                break;
            }
            case dp::world::ResourceKind::Grass: {
                // 3 short cones in a tuft
                Color gc = tint_mul(Color{80, 180, 80, 255}, tint);
                Vector3 b = Vector3{ r.x, r.y, r.z };
                Vector3 t1 = Vector3{ r.x,        r.y + 0.05f, r.z + 0.40f };
                Vector3 t2 = Vector3{ r.x + 0.10f,r.y - 0.05f, r.z + 0.35f };
                Vector3 t3 = Vector3{ r.x - 0.10f,r.y - 0.05f, r.z + 0.30f };
                DrawCylinderEx(b, t1, 0.05f, 0.0f, 4, gc);
                DrawCylinderEx(b, t2, 0.05f, 0.0f, 4, gc);
                DrawCylinderEx(b, t3, 0.05f, 0.0f, 4, gc);
                break;
            }
        }
    }

    // Construction sites: a white wireframe footprint + a progressively
    // built-up half-finished house, plus 3 material progress bars. Stages:
    //   <25%: just the footprint and 3 material stacks
    //   25-50%: footprint + low foundation slab
    //   50-75%: foundation + walls growing
    //   75-100%: full-height walls, no roof yet
    // Once the site finalises BuildingSystem promotes it to a real
    // Building (which gets its own roof + coverage ring further below).
    for (const auto& s : buildings.sites()) {
        if (s.completed) continue;
        const auto& r = dp::world::building_recipe(s.type);
        float w = r.foot_radius * 2.0f;
        float p = s.material_progress();

        // 1. Footprint frame is always drawn (so empty plots are visible).
        Vector3 frame_c = Vector3{ s.x, s.y, s.z + 0.05f };
        DrawCubeWiresV(frame_c, Vector3{ w, w, 0.10f },
                       Color{ 240, 240, 240, 220 });
        DrawCubeV(frame_c, Vector3{ w, w, 0.10f },
                  Color{ 200, 200, 200, 60 });

        // 2. Foundation slab (>=25% progress).
        if (p >= 0.25f) {
            Color stone_col = tint_mul(Color{140, 140, 145, 255}, tint);
            DrawCubeV(Vector3{ s.x, s.y, s.z + 0.20f },
                      Vector3{ w * 0.95f, w * 0.95f, 0.30f },
                      stone_col);
        }

        // 3. Walls growing (>=50%). Their height scales from 0 -> full
        // as progress goes 50% -> 100%.
        if (p >= 0.50f) {
            float wall_p = (p - 0.50f) / 0.50f;  // 0..1 in this band
            if (wall_p > 1.0f) wall_p = 1.0f;
            // Per-type full wall height (matches the completed building).
            float full_h;
            Color wall_col;
            switch (s.type) {
                case dp::world::BuildingType::Shed:
                    full_h = 1.6f; wall_col = Color{180, 130, 70, 255};
                    break;
                case dp::world::BuildingType::WoodHouse:
                    full_h = 2.6f; wall_col = Color{160, 110, 60, 255};
                    break;
                case dp::world::BuildingType::StoneHouse:
                    full_h = 2.6f; wall_col = Color{160, 160, 165, 255};
                    break;
                case dp::world::BuildingType::BigHouse:
                    full_h = 3.4f; wall_col = Color{170, 150, 120, 255};
                    break;
            }
            float ch = full_h * wall_p;
            if (ch > 0.01f) {
                DrawCubeV(Vector3{ s.x, s.y, s.z + 0.35f + ch * 0.5f },
                          Vector3{ w * 0.95f, w * 0.95f, ch },
                          tint_mul(wall_col, tint));
                DrawCubeWiresV(Vector3{ s.x, s.y, s.z + 0.35f + ch * 0.5f },
                               Vector3{ w * 0.95f, w * 0.95f, ch },
                               Color{50, 40, 30, 200});
            }
        }

        // 4. Material progress bars (always visible until completed).
        // Show them off to the side so they don't clip the half-house.
        auto bar = [&](int idx, int deposited, int required, Color col) {
            if (required <= 0) return;
            float f = static_cast<float>(deposited) / required;
            if (f > 1.0f) f = 1.0f;
            float bw = 0.20f;
            float bx = s.x + (idx - 1) * 0.30f;
            float by = s.y + r.foot_radius + 0.4f;
            float bz_low = s.z + 0.10f;
            float bh = 1.5f * f;
            Vector3 center = Vector3{ bx, by, bz_low + bh * 0.5f };
            Vector3 size = Vector3{ bw, bw, std::max(0.05f, bh) };
            DrawCubeV(center, size, tint_mul(col, tint));
        };
        bar(0, s.wood_deposited,  r.wood_required,  Color{160, 110, 50, 255});
        bar(1, s.stone_deposited, r.stone_required, Color{170, 170, 170, 255});
        bar(2, s.grass_deposited, r.grass_required, Color{ 80, 200, 80, 255});
    }

    // Buildings: completed houses. If a Quaternius GLB loaded for the
    // type, render it; otherwise fall back to the colored geometry box.
    for (const auto& b : buildings.buildings()) {
        const auto& r = dp::world::building_recipe(b.type);
        float w  = r.foot_radius * 2.0f;
        int   tidx = static_cast<int>(b.type);

        if (tidx >= 0 && tidx < 4 && building_models_loaded_[tidx]) {
            // Per-type uniform scale: Quaternius assets are roughly
            // unit-sized, so we scale by foot_radius to match the
            // recipe's footprint.
            float scale = r.foot_radius * 1.6f;
            DrawModelEx(building_models_[tidx],
                        Vector3{ b.x, b.y, b.z },
                        Vector3{ 1.0f, 0.0f, 0.0f },  // X axis
                        90.0f,                         // rotate to Z-up
                        Vector3{ scale, scale, scale },
                        tint);
        } else {
            float h;
            Color wall_col, roof_col;
            switch (b.type) {
                case dp::world::BuildingType::Shed:
                    h = 1.6f;
                    wall_col = Color{180, 130, 70, 255};
                    roof_col = Color{120,  80, 50, 255};
                    break;
                case dp::world::BuildingType::WoodHouse:
                    h = 2.6f;
                    wall_col = Color{160, 110, 60, 255};
                    roof_col = Color{100,  70, 40, 255};
                    break;
                case dp::world::BuildingType::StoneHouse:
                    h = 2.6f;
                    wall_col = Color{160, 160, 165, 255};
                    roof_col = Color{120, 120, 130, 255};
                    break;
                case dp::world::BuildingType::BigHouse:
                    h = 3.4f;
                    wall_col = Color{170, 150, 120, 255};
                    roof_col = Color{100, 100, 130, 255};
                    break;
            }
            Vector3 walls_c = Vector3{ b.x, b.y, b.z + h * 0.5f };
            Vector3 walls_s = Vector3{ w, w, h };
            DrawCubeV(walls_c, walls_s, tint_mul(wall_col, tint));
            DrawCubeWiresV(walls_c, walls_s, Color{50, 40, 30, 200});
            Vector3 roof_c = Vector3{ b.x, b.y, b.z + h + 0.15f };
            Vector3 roof_s = Vector3{ w * 1.15f, w * 1.15f, 0.30f };
            DrawCubeV(roof_c, roof_s, tint_mul(roof_col, tint));
        }

        // Coverage ring on ground (faded yellow circle) - always shown
        DrawCircle3D(Vector3{ b.x, b.y, b.z + 0.05f }, b.coverage_radius,
                     Vector3{1, 0, 0}, 90.0f,
                     Color{ 240, 220, 120, 100 });
    }

    // Farm plots: small dirt square; sprout grows taller as it matures;
    // ripe plots have a brighter coloured fruit on top.
    for (const auto& fp : farming.plots()) {
        Color dirt = tint_mul(Color{ 96, 70, 50, 255 }, tint);
        Vector3 c = Vector3{ fp.x, fp.y, fp.z + 0.05f };
        DrawCubeV(c, Vector3{ 1.2f, 1.2f, 0.10f }, dirt);
        // Water level shows as a slightly bluish overlay
        if (fp.water_level > 0.05f) {
            Color wet = Color{ 60, 80, 200,
                static_cast<unsigned char>(80 * fp.water_level) };
            DrawCubeV(c, Vector3{ 1.2f, 1.2f, 0.11f },
                      tint_mul(wet, tint));
        }
        // Sprout height = growth
        float h = 0.10f + fp.growth * 0.80f;
        Color stalk_col = tint_mul(Color{ 80, 180, 80, 255 }, tint);
        DrawCylinderEx(Vector3{ fp.x, fp.y, fp.z },
                       Vector3{ fp.x, fp.y, fp.z + h },
                       0.06f, 0.04f, 6, stalk_col);
        if (fp.ripe) {
            Color fruit_col;
            switch (fp.seed_kind) {
                case dp::world::PlantKind::Fruit:    fruit_col = Color{255, 170,  30, 255}; break;
                case dp::world::PlantKind::Berry:    fruit_col = Color{220,  50,  90, 255}; break;
                case dp::world::PlantKind::Cactus:   fruit_col = Color{100, 220, 150, 255}; break;
                case dp::world::PlantKind::Mushroom: fruit_col = Color{210, 210,  90, 255}; break;
                default:                              fruit_col = Color{255, 200, 100, 255}; break;
            }
            DrawSphere(Vector3{ fp.x, fp.y, fp.z + h }, 0.18f,
                       tint_mul(fruit_col, tint));
        }
    }

    // Shelter: a simple A-frame lean-to (two angled cylinders + roof)
    if (shelter.placed()) {
        float sx = shelter.x();
        float sy = shelter.y();
        float sz = shelter.z();
        Color wood_col = tint_mul(Color{150, 105, 60, 255}, tint);
        Color roof_col = tint_mul(Color{ 90, 140, 70, 200}, tint);
        // Two angled support poles forming the A
        Vector3 base_a = Vector3{ sx - 1.2f, sy, sz };
        Vector3 base_b = Vector3{ sx + 1.2f, sy, sz };
        Vector3 ridge = Vector3{ sx, sy, sz + 1.6f };
        DrawCylinderEx(base_a, ridge, 0.10f, 0.06f, 6, wood_col);
        DrawCylinderEx(base_b, ridge, 0.10f, 0.06f, 6, wood_col);
        // Grass roof: a flat slab between the two slopes
        DrawCubeV(Vector3{ sx, sy, sz + 0.85f },
                  Vector3{ 2.4f, 1.6f, 0.10f }, roof_col);
        // Coverage circle (flat disk to suggest shelter radius)
        DrawCircle3D(Vector3{ sx, sy, sz + 0.05f }, shelter.radius(),
                     Vector3{1, 0, 0}, 90.0f,
                     Color{ 240, 220, 120, 120 });
    }

    // Agent capsule (white when above ground, cyan when underground).
    // If wearing a grass dress / fur cloak we wrap a coloured "skirt"
    // around the lower body so progression is visible from any angle.
    const auto& s = agent.state();
    Vector3 foot   = Vector3{ s.pos_x, s.pos_y, s.pos_z };
    Vector3 head   = Vector3{ s.pos_x, s.pos_y, s.pos_z + s.height };
    Color body_col = agent.inside_cave() ? Color{ 80, 220, 220, 255 }
                                         : Color{ 240, 240, 240, 255 };
    DrawCapsule(foot, head, s.radius, 12, 4, body_col);

    // Clothing visual: a wider tinted band at hip height
    if (worn_clothing == dp::agent::ItemKind::GrassDress
     || worn_clothing == dp::agent::ItemKind::FurCloak) {
        Color cloth_col = (worn_clothing == dp::agent::ItemKind::FurCloak)
            ? Color{120,  80, 50, 230}      // warm brown
            : Color{ 80, 170, 60, 230};     // grass green
        float skirt_z_low  = s.pos_z + s.height * 0.30f;
        float skirt_z_high = s.pos_z + s.height * 0.55f;
        Vector3 sk_lo = Vector3{ s.pos_x, s.pos_y, skirt_z_low };
        Vector3 sk_hi = Vector3{ s.pos_x, s.pos_y, skirt_z_high };
        DrawCylinderEx(sk_lo, sk_hi,
                       s.radius * 1.25f,
                       s.radius * 1.05f,
                       16,
                       tint_mul(cloth_col, tint));
    }

    // Origin marker (helps orientation)
    DrawCubeV(Vector3{ 0, 0, 1 }, Vector3{ 1, 1, 2 }, Color{ 230, 30, 30, 255 });

    EndMode3D();

    (void)world;
}

}  // namespace dp::render
