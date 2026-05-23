#include "world/PlantSystem.hpp"

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

std::vector<PlantKind> PlantSystem::try_harvest(float x, float y, float z, float radius) {
    std::vector<PlantKind> out;
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
        }
    }
    return out;
}

}  // namespace dp::world
