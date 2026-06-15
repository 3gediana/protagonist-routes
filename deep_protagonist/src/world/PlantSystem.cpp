#include "world/PlantSystem.hpp"

#include <cmath>

#include <spdlog/spdlog.h>

namespace dp::world {

const char* plant_kind_name(PlantKind k) {
    switch (k) {
        case PlantKind::Berry:    return "berry";
        case PlantKind::Fruit:    return "fruit";
        case PlantKind::Cactus:   return "cactus";
        case PlantKind::Mushroom: return "mushroom";
    }
    return "?";
}

float PlantSystem::frand() {
    rng_state_ ^= rng_state_ >> 12;
    rng_state_ ^= rng_state_ << 25;
    rng_state_ ^= rng_state_ >> 27;
    uint64_t r = rng_state_ * 0x2545F4914F6CDD1DULL;
    return static_cast<float>(r & 0xFFFFFFu) / 16777216.0f;
}

float PlantSystem::regrow_for(PlantKind k) const {
    switch (k) {
        case PlantKind::Berry:    return berry_regrow_seconds;
        case PlantKind::Fruit:    return fruit_regrow_seconds;
        case PlantKind::Cactus:   return cactus_regrow_seconds;
        case PlantKind::Mushroom: return mushroom_regrow_seconds;
    }
    return 60.0f;
}

void PlantSystem::initialize(uint64_t seed, const World& world,
                             const Decorations& decorations) {
    rng_state_ = seed ? seed : 1;
    plants_.clear();
    plants_.reserve(decorations.items().size() / 2);

    // Each Tree decoration in a Forest biome gets fruit; each Cactus has
    // edible flesh; each Rock or generic decoration may host a berry bush
    // (sparse) or mushroom (in swamp). We piggy-back on Decorations to keep
    // visual placement consistent with the static world.
    for (const auto& d : decorations.items()) {
        Biome b = world.biome_at(d.x, d.y);
        switch (d.kind) {
            case DecorationKind::Tree:
                if (b == Biome::Forest && frand() < 0.7f) {
                    plants_.push_back({d.x, d.y, d.z + 1.5f, PlantKind::Fruit, true, 0.0f});
                } else if (b == Biome::Plain && frand() < 0.4f) {
                    plants_.push_back({d.x, d.y, d.z + 0.6f, PlantKind::Berry, true, 0.0f});
                } else if (b == Biome::Swamp && frand() < 0.5f) {
                    plants_.push_back({d.x, d.y, d.z + 0.4f, PlantKind::Mushroom, true, 0.0f});
                }
                break;
            case DecorationKind::Cactus:
                plants_.push_back({d.x, d.y, d.z + 1.0f, PlantKind::Cactus, true, 0.0f});
                break;
            case DecorationKind::Rock:
                if (b == Biome::Plain && frand() < 0.15f) {
                    plants_.push_back({d.x + 0.5f, d.y + 0.5f, d.z + 0.3f, PlantKind::Berry, true, 0.0f});
                }
                break;
        }
    }

    // D-050 P4: blueprint line 22 explicitly puts berry bushes ("浆果灌木")
    // alongside the early-stage forage list, but the original generator
    // tied every plant to a Decoration item so river banks - exactly the
    // place an early survivor needs both food *and* water - ended up
    // empty whenever no Tree/Rock happened to land there.
    //
    // D-050 P5: instead of stamping berries adjacent to River cells
    // (which fails when several rivers run parallel and every
    // 4-neighbour ends up in water), we sweep every dry-land cell on a
    // 4 m grid and check whether *any* river/swamp cell exists within
    // 12 m. This guarantees that every metre of riverbank gets a couple
    // of berry bushes within the agent's 60 m vision range.
    {
        float wx_max = world.heightmap().world_size_x();
        float wy_max = world.heightmap().world_size_y();
        const float step = 4.0f;
        const float probe_r = 12.0f;
        const float probe_r2 = probe_r * probe_r;
        for (float wy = step * 0.5f; wy < wy_max; wy += step) {
            for (float wx = step * 0.5f; wx < wx_max; wx += step) {
                Biome b = world.biome_at(wx, wy);
                if (b == Biome::River || b == Biome::Swamp) continue;
                // Any river/swamp cell within 12 m?
                bool near_water = false;
                for (float dy = -probe_r; dy <= probe_r
                                       && !near_water; dy += step) {
                    for (float dx = -probe_r; dx <= probe_r
                                           && !near_water; dx += step) {
                        if (dx*dx + dy*dy > probe_r2) continue;
                        Biome nb = world.biome_at(wx + dx, wy + dy);
                        if (nb == Biome::River
                         || nb == Biome::Swamp) near_water = true;
                    }
                }
                if (!near_water) continue;
                // 60 % per 4 m cell. A 60 m vision window contains
                // hundreds of 4 m cells along an average riverbank, so
                // even at 60 % density the agent's first frame already
                // sees plenty of berries from any water-adjacent spawn.
                if (frand() < 0.60f) {
                    float wz = world.surface_height(wx, wy) + 0.4f;
                    plants_.push_back({wx, wy, wz, PlantKind::Berry, true, 0.0f});
                }
            }
        }
    }
    int berry = 0, fruit = 0, mush = 0, cact = 0;
    for (const auto& p : plants_) {
        switch (p.kind) {
            case PlantKind::Berry:    ++berry; break;
            case PlantKind::Fruit:    ++fruit; break;
            case PlantKind::Mushroom: ++mush;  break;
            case PlantKind::Cactus:   ++cact;  break;
        }
    }
    spdlog::info("PlantSystem D-050 P4: total={} (berry={} fruit={} mush={} cact={})",
                 plants_.size(), berry, fruit, mush, cact);
}

void PlantSystem::tick(float dt) {
    for (auto& p : plants_) {
        if (!p.ripe) {
            p.regrow_timer -= dt;
            if (p.regrow_timer <= 0.0f) {
                p.ripe = true;
                p.regrow_timer = 0.0f;
            }
        }
    }
}

// ---- D-132 forage scarcity ------------------------------------------------

void PlantSystem::configure_scarcity(const ScarcityCfg& c, const World* world) {
    scar_ = c;
    scar_world_ = world;
    redist_timer_ = 0.0f;
}

void PlantSystem::relocate_plant_(PlantInstance& p,
                                  float avoid_x, float avoid_y, float avoid_r) {
    if (!scar_world_) return;
    const float wx_max = scar_world_->heightmap().world_size_x();
    const float wy_max = scar_world_->heightmap().world_size_y();
    const float br2 = scar_.barren_r * scar_.barren_r;
    const float ar2 = avoid_r * avoid_r;
    // Preferred: dry land outside BOTH the spawn barren ring and the avoid ring.
    for (int attempts = 0; attempts < 32; ++attempts) {
        float x = frand() * wx_max;
        float y = frand() * wy_max;
        float dsx = x - scar_.spawn_x, dsy = y - scar_.spawn_y;
        if (dsx * dsx + dsy * dsy < br2) continue;
        float dax = x - avoid_x, day = y - avoid_y;
        if (dax * dax + day * day < ar2) continue;
        Biome b = scar_world_->biome_at(x, y);
        if (b == Biome::River || b == Biome::Swamp) continue;
        float z = scar_world_->surface_height(x, y);
        if (scar_world_->is_underground(x, y, z + 0.5f)) continue;
        p.x = x; p.y = y; p.z = z + 0.5f;
        return;
    }
    // Fallback: any dry-land spot outside the spawn barren ring.
    for (int attempts = 0; attempts < 16; ++attempts) {
        float x = frand() * wx_max;
        float y = frand() * wy_max;
        float dsx = x - scar_.spawn_x, dsy = y - scar_.spawn_y;
        if (dsx * dsx + dsy * dsy < br2) continue;
        Biome b = scar_world_->biome_at(x, y);
        if (b == Biome::River || b == Biome::Swamp) continue;
        p.x = x; p.y = y; p.z = scar_world_->surface_height(x, y) + 0.5f;
        return;
    }
    // Give up: leave the plant where it is (rare).
}

void PlantSystem::apply_initial_scarcity() {
    if (!scar_.enabled || !scar_world_) return;
    for (auto& p : plants_) {
        relocate_plant_(p, scar_.spawn_x, scar_.spawn_y, scar_.barren_r);
        p.ripe = true;
        p.regrow_timer = 0.0f;
    }
    redist_timer_ = 0.0f;
}

float PlantSystem::nearest_ripe_dist(float x, float y) const {
    float best2 = 1e18f;
    for (const auto& p : plants_) {
        if (!p.ripe) continue;
        float dx = p.x - x, dy = p.y - y;
        float d2 = dx * dx + dy * dy;
        if (d2 < best2) best2 = d2;
    }
    return (best2 >= 1e18f) ? -1.0f : std::sqrt(best2);
}

void PlantSystem::tick(float dt, float agent_x, float agent_y) {
    tick(dt);  // ordinary in-place regrow
    if (!scar_.enabled || !scar_world_ || scar_.redist_sec <= 0.0f) return;
    redist_timer_ += dt;
    if (redist_timer_ >= scar_.redist_sec) {
        redist_timer_ = 0.0f;
        // Re-scatter the whole forage field to fresh land, pushed away from
        // wherever the agent currently is so the resources never sit on his
        // lap. Ripeness/regrow state is preserved.
        for (auto& p : plants_) {
            relocate_plant_(p, agent_x, agent_y, scar_.barren_r);
        }
    }
}

std::vector<PlantKind> PlantSystem::try_harvest(float x, float y, float z, float radius,
                                                bool is_night) {
    std::vector<PlantKind> out;
    // D-092: 夜里采不到野果. Plants stay in place visually (the agent still
    // sees them in obs), but try_harvest refuses to pick anything until
    // sunrise. This nudges PPO toward the blueprint's day/night cycle:
    // forage during day, retreat to shelter at night.
    if (is_night) return out;
    float r2 = radius * radius;
    for (auto& p : plants_) {
        if (!p.ripe) continue;
        float dx = p.x - x;
        float dy = p.y - y;
        float dz = p.z - z;
        if (dx * dx + dy * dy + dz * dz <= r2) {
            out.push_back(p.kind);
            p.ripe = false;
            p.regrow_timer = regrow_for(p.kind);
            // D-132: vicinity depletes -- the eaten plant regrows far away
            // (outside the barren ring around the agent) instead of in place,
            // so the agent cannot camp a single bush.
            if (scar_.enabled && scar_world_) {
                relocate_plant_(p, x, y, scar_.barren_r);
            }
        }
    }
    return out;
}

}  // namespace dp::world
