#include "world/ResourceSystem.hpp"

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
bool wood_biome(Biome b) {
    // Sticks are common in forests, sparse in plains, occasional in swamps.
    return b == Biome::Forest || b == Biome::Plain || b == Biome::Swamp;
}
bool stone_biome(Biome b) {
    return b == Biome::Mountain || b == Biome::Plain || b == Biome::Desert;
}
bool grass_biome(Biome b) {
    return b == Biome::Plain || b == Biome::Forest || b == Biome::Swamp;
}
}  // namespace

void ResourceSystem::scatter(const World& w, ResourceKind kind,
                             int target_count,
                             bool (*biome_filter)(Biome)) {
    float wx_max = w.heightmap().world_size_x();
    float wy_max = w.heightmap().world_size_y();
    int placed = 0;
    int attempts = 0;
    int max_attempts = target_count * 10;
    while (placed < target_count && attempts < max_attempts) {
        ++attempts;
        float x = frand() * wx_max;
        float y = frand() * wy_max;
        Biome b = w.biome_at(x, y);
        if (!biome_filter(b)) continue;
        // Skip rivers explicitly even if other filters match.
        if (b == Biome::River) continue;
        // Drop on top of the surface; resources don't go underwater.
        float z = w.surface_height(x, y);
        ResourceInstance r;
        r.x = x;
        r.y = y;
        r.z = z;
        r.kind = kind;
        r.available = true;
        r.regrow_timer = 0.0f;
        resources_.push_back(r);
        ++placed;
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
