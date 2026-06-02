#include "world/ResourceSystem.hpp"

#include <algorithm>
#include <cmath>

namespace dp::world {

const char* resource_kind_name(ResourceKind k) {
    switch (k) {
        case ResourceKind::Wood:  return "wood";
        case ResourceKind::Stone: return "stone";
        case ResourceKind::Grass: return "grass";
    }
    return "?";
}

float ResourceSystem::frand() {
    rng_state_ ^= rng_state_ >> 12;
    rng_state_ ^= rng_state_ << 25;
    rng_state_ ^= rng_state_ >> 27;
    uint64_t r = rng_state_ * 0x2545F4914F6CDD1DULL;
    return static_cast<float>(r & 0xFFFFFFu) / 16777216.0f;
}

float ResourceSystem::regrow_for(ResourceKind k) const {
    switch (k) {
        case ResourceKind::Wood:  return wood_regrow_seconds;
        case ResourceKind::Stone: return stone_regrow_seconds;
        case ResourceKind::Grass: return grass_regrow_seconds;
    }
    return 60.0f;
}

namespace {
// Region-exclusive resources: each biome holds a distinct main-line necessity
// so no single region is self-sufficient. The agent must traverse the whole map
// (wood->forest, stone->mountain, fibre/grass->plains+swamp) to climb the
// milestone line (collect -> spear -> shelter -> clothing -> house). Water (river)
// and herbivores (plains/forest) supply drink and protein, both elsewhere again.
// This keeps every region valuable without touching the frozen obs/action contract.
bool wood_biome(Biome b) {
    // Building/spear-shaft timber: forests only.
    return b == Biome::Forest;
}
bool stone_biome(Biome b) {
    // Spear-tip / tools / house foundation: mountains only.
    return b == Biome::Mountain;
}
bool grass_biome(Biome b) {
    // Clothing fibre (grass weave): open plains and wet swamps.
    return b == Biome::Plain || b == Biome::Swamp;
}
}  // namespace

void ResourceSystem::scatter(const World& w, ResourceKind kind,
                             int target_count,
                             bool (*biome_filter)(Biome)) {
    float wx_max = w.heightmap().world_size_x();
    float wy_max = w.heightmap().world_size_y();
    if (target_count <= 0) return;

    auto place_at = [&](float x, float y) -> bool {
        Biome b = w.biome_at(x, y);
        if (!biome_filter(b)) return false;
        if (b == Biome::River) return false;  // resources don't go in water
        ResourceInstance r;
        r.x = x;
        r.y = y;
        r.z = w.surface_height(x, y);  // sit on the surface
        r.kind = kind;
        r.available = true;
        r.regrow_timer = 0.0f;
        resources_.push_back(r);
        return true;
    };

    // Clustered ("patchy") layout instead of a uniform sprinkle: real worlds
    // have groves of sticks, veins/scree of stone, meadows of grass - dense
    // pockets separated by empty ground - rather than evenly-spread dots. We
    // seed a handful of patch centres in valid biomes and grow each into a
    // blob, which gives the map geographic character without changing the
    // total node budget. Stone clumps tightest (veins), grass spreads widest
    // (meadows).
    float patch_radius;
    int   nodes_per_patch;
    switch (kind) {
        case ResourceKind::Stone: patch_radius = 0.018f; nodes_per_patch = 10; break;
        case ResourceKind::Wood:  patch_radius = 0.030f; nodes_per_patch = 14; break;
        case ResourceKind::Grass: patch_radius = 0.050f; nodes_per_patch = 18; break;
        default:                  patch_radius = 0.030f; nodes_per_patch = 14; break;
    }
    float world_span = 0.5f * (wx_max + wy_max);
    float spread = patch_radius * world_span;

    int placed = 0;
    int patch_guard = 0;
    int max_patches = (target_count / std::max(1, nodes_per_patch)) * 4 + 8;
    while (placed < target_count && patch_guard < max_patches) {
        ++patch_guard;
        // Find a patch centre that lands in a valid biome (best-effort).
        float cx = 0.0f, cy = 0.0f;
        bool centre_ok = false;
        for (int t = 0; t < 48; ++t) {
            cx = frand() * wx_max;
            cy = frand() * wy_max;
            Biome b = w.biome_at(cx, cy);
            if (biome_filter(b) && b != Biome::River) { centre_ok = true; break; }
        }
        if (!centre_ok) continue;
        // Grow this patch: jittered blob around the centre. Members that fall
        // on an invalid cell (e.g. patch straddles a biome edge) are skipped,
        // which naturally feathers the patch along terrain boundaries.
        int want = nodes_per_patch + static_cast<int>(frand() * nodes_per_patch);
        for (int k = 0; k < want && placed < target_count; ++k) {
            float ang = frand() * 6.2831853f;
            float rad = spread * std::sqrt(frand());  // uniform-in-disc
            float x = std::min(std::max(cx + std::cos(ang) * rad, 0.0f), wx_max);
            float y = std::min(std::max(cy + std::sin(ang) * rad, 0.0f), wy_max);
            if (place_at(x, y)) ++placed;
        }
    }
    // Top up uniformly if the patches couldn't satisfy the budget (e.g. a
    // biome is rare on this seed), so counts stay deterministic per seed.
    int fill_guard = 0;
    while (placed < target_count && fill_guard < target_count * 20) {
        ++fill_guard;
        if (place_at(frand() * wx_max, frand() * wy_max)) ++placed;
    }
}

void ResourceSystem::initialize(uint64_t seed, const World& w) {
    rng_state_ = seed ? seed : 0xC0FFEE13ULL;
    resources_.clear();
    // Scale the targets down for tiny test worlds (16-cell scratch maps).
    float wx = w.heightmap().world_size_x();
    float wy = w.heightmap().world_size_y();
    float area_factor = (wx * wy) / (1024.0f * 1024.0f);
    if (area_factor < 0.05f) area_factor = 0.05f;
    int wood   = std::max(8, int(wood_target_count  * area_factor));
    int stone  = std::max(8, int(stone_target_count * area_factor));
    int grass  = std::max(8, int(grass_target_count * area_factor));
    scatter(w, ResourceKind::Wood,  wood,  wood_biome);
    scatter(w, ResourceKind::Stone, stone, stone_biome);
    scatter(w, ResourceKind::Grass, grass, grass_biome);
}

void ResourceSystem::tick(float dt) {
    for (auto& r : resources_) {
        if (r.available) continue;
        r.regrow_timer -= dt;
        if (r.regrow_timer <= 0.0f) {
            r.available = true;
            r.regrow_timer = 0.0f;
        }
    }
}

std::vector<ResourceKind> ResourceSystem::try_collect(float x, float y,
                                                      float z, float radius) {
    std::vector<ResourceKind> out;
    float r2 = radius * radius;
    for (auto& r : resources_) {
        if (!r.available) continue;
        float dx = r.x - x, dy = r.y - y, dz = r.z - z;
        if (dx*dx + dy*dy + dz*dz <= r2) {
            r.available = false;
            r.regrow_timer = regrow_for(r.kind);
            out.push_back(r.kind);
        }
    }
    return out;
}

int ResourceSystem::available_count(ResourceKind k) const {
    int n = 0;
    for (const auto& r : resources_) {
        if (r.available && r.kind == k) ++n;
    }
    return n;
}

}  // namespace dp::world
