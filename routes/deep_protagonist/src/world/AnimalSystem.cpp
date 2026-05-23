#include "world/AnimalSystem.hpp"

#include <algorithm>
#include <cmath>

namespace dp::world {

const char* animal_kind_name(AnimalKind k) {
    switch (k) {
        case AnimalKind::Rabbit: return "rabbit";
        case AnimalKind::Deer:   return "deer";
        case AnimalKind::Wolf:   return "wolf";
        case AnimalKind::Fish:   return "fish";
        case AnimalKind::Eagle:  return "eagle";
    }
    return "?";
}

float AnimalSystem::frand() {
    rng_state_ ^= rng_state_ >> 12;
    rng_state_ ^= rng_state_ << 25;
    rng_state_ ^= rng_state_ >> 27;
    uint64_t r = rng_state_ * 0x2545F4914F6CDD1DULL;
    return static_cast<float>(r & 0xFFFFFFu) / 16777216.0f;
}

namespace {
struct KindTraits {
    Biome preferred;        // initial spawn biome (best-effort)
    float walk_speed;       // normal speed
    float panic_speed;      // when fleeing or hunting
    float perception_range; // how far it senses agent / prey
};

KindTraits traits_for(AnimalKind k) {
    switch (k) {
        case AnimalKind::Rabbit: return {Biome::Plain,    2.0f, 6.0f, 25.0f};
        case AnimalKind::Deer:   return {Biome::Forest,   2.5f, 7.0f, 35.0f};
        case AnimalKind::Wolf:   return {Biome::Mountain, 3.0f, 6.5f, 40.0f};
        case AnimalKind::Fish:   return {Biome::River,    1.0f, 1.5f, 10.0f};
        case AnimalKind::Eagle:  return {Biome::Mountain, 5.0f, 5.0f,  0.0f};
    }
    return {Biome::Plain, 2.0f, 5.0f, 20.0f};
}
}  // namespace

void AnimalSystem::spawn(AnimalInstance& a, AnimalKind k, const World& world) {
    auto t = traits_for(k);
    a.kind = k;
    a.state = AnimalState::Wander;
    a.health = 100.0f;
    a.state_timer = 0.0f;

    // Find a spawn cell whose biome is "compatible". Best-effort: try 32
    // random samples, fall back to anywhere if no match.
    float wx_max = world.heightmap().world_size_x();
    float wy_max = world.heightmap().world_size_y();
    int   ok = -1;
    for (int i = 0; i < 32; ++i) {
        float x = frand() * wx_max;
        float y = frand() * wy_max;
        Biome b = world.biome_at(x, y);
        bool match = (b == t.preferred)
                  || (k == AnimalKind::Fish  && (b == Biome::River || b == Biome::Swamp))
                  || (k == AnimalKind::Eagle && (b == Biome::Mountain || b == Biome::Plain))
                  || (k == AnimalKind::Wolf  && (b == Biome::Mountain || b == Biome::Forest));
        if (match) {
            a.pos_x = x; a.pos_y = y;
            ok = 1; break;
        }
    }
    if (ok < 0) {
        a.pos_x = frand() * wx_max;
        a.pos_y = frand() * wy_max;
    }
    a.pos_z = world.surface_height(a.pos_x, a.pos_y) + 0.5f;
    if (k == AnimalKind::Eagle) a.pos_z += 25.0f;  // hovering altitude

    a.yaw = frand() * 6.2831853f;
    a.wander_x = std::cos(a.yaw);
    a.wander_y = std::sin(a.yaw);
}

void AnimalSystem::initialize(uint64_t seed, const World& world,
                              int rabbits, int deer, int wolves, int fish, int eagles) {
    rng_state_ = seed ? seed : 1;
    animals_.clear();
    animals_.reserve(rabbits + deer + wolves + fish + eagles);
    auto add = [&](AnimalKind k, int n) {
        for (int i = 0; i < n; ++i) {
            AnimalInstance a;
            spawn(a, k, world);
            animals_.push_back(a);
        }
    };
    add(AnimalKind::Rabbit, rabbits);
    add(AnimalKind::Deer,   deer);
    add(AnimalKind::Wolf,   wolves);
    add(AnimalKind::Fish,   fish);
    add(AnimalKind::Eagle,  eagles);
}

// ---------------------------------------------------------------------------
// Per-kind state machines
// ---------------------------------------------------------------------------

void AnimalSystem::integrate_with_terrain(AnimalInstance& a, const World& world, float dt) {
    a.pos_x += a.vel_x * dt;
    a.pos_y += a.vel_y * dt;
    a.pos_z = world.surface_height(a.pos_x, a.pos_y) + 0.5f;
    // Clamp inside world
    float wx_max = world.heightmap().world_size_x();
    float wy_max = world.heightmap().world_size_y();
    a.pos_x = std::clamp(a.pos_x, 0.0f, wx_max);
    a.pos_y = std::clamp(a.pos_y, 0.0f, wy_max);
    if (std::abs(a.vel_x) + std::abs(a.vel_y) > 0.01f) {
        a.yaw = std::atan2(a.vel_y, a.vel_x);
    }
}

void AnimalSystem::tick_rabbit(AnimalInstance& a, const World& world,
                               float ax, float ay, float dt) {
    auto t = traits_for(AnimalKind::Rabbit);
    float dx = ax - a.pos_x, dy = ay - a.pos_y;
    float d2 = dx * dx + dy * dy;
    bool agent_close = d2 < t.perception_range * t.perception_range;

    if (agent_close) {
        a.state = AnimalState::Flee;
        // Run away from agent
        float d = std::sqrt(d2) + 1e-3f;
        a.vel_x = -dx / d * t.panic_speed;
        a.vel_y = -dy / d * t.panic_speed;
        a.state_timer = 1.0f;
    } else {
        a.state_timer -= dt;
        if (a.state_timer <= 0.0f) {
            a.state = AnimalState::Wander;
            float angle = frand() * 6.2831853f;
            a.wander_x = std::cos(angle);
            a.wander_y = std::sin(angle);
            a.state_timer = 1.5f + frand() * 1.5f;
        }
        a.vel_x = a.wander_x * t.walk_speed;
        a.vel_y = a.wander_y * t.walk_speed;
    }
    integrate_with_terrain(a, world, dt);
}

void AnimalSystem::tick_deer(AnimalInstance& a, const World& world,
                             float ax, float ay, float dt) {
    auto t = traits_for(AnimalKind::Deer);
    float dx = ax - a.pos_x, dy = ay - a.pos_y;
    float d2 = dx * dx + dy * dy;
    bool agent_close = d2 < t.perception_range * t.perception_range;

    if (agent_close) {
        a.state = AnimalState::Flee;
        float d = std::sqrt(d2) + 1e-3f;
        a.vel_x = -dx / d * t.panic_speed;
        a.vel_y = -dy / d * t.panic_speed;
        a.state_timer = 2.0f;
    } else {
        a.state_timer -= dt;
        if (a.state_timer <= 0.0f) {
            a.state = AnimalState::Wander;
            float angle = frand() * 6.2831853f;
            a.wander_x = std::cos(angle);
            a.wander_y = std::sin(angle);
            a.state_timer = 2.5f + frand() * 2.5f;
        }
        a.vel_x = a.wander_x * t.walk_speed;
        a.vel_y = a.wander_y * t.walk_speed;
    }
    integrate_with_terrain(a, world, dt);
}

void AnimalSystem::tick_wolf(AnimalInstance& a, const World& world,
                             float ax, float ay, float dt, float night_factor) {
    auto t = traits_for(AnimalKind::Wolf);
    // Nocturnal aggression: at night, wolves notice the agent up to 50%
    // farther away and chase 20% faster. night_factor is clamped [0,1].
    if (night_factor < 0.0f) night_factor = 0.0f;
    if (night_factor > 1.0f) night_factor = 1.0f;
    float perception = t.perception_range * (1.0f + 0.5f * night_factor);
    float speed      = t.panic_speed     * (1.0f + 0.2f * night_factor);

    float dx = ax - a.pos_x, dy = ay - a.pos_y;
    float d2 = dx * dx + dy * dy;
    bool agent_in_range = d2 < perception * perception;

    if (agent_in_range) {
        a.state = AnimalState::Hunt;
        float d = std::sqrt(d2) + 1e-3f;
        a.vel_x = dx / d * speed;
        a.vel_y = dy / d * speed;
    } else {
        a.state_timer -= dt;
        if (a.state_timer <= 0.0f) {
            a.state = AnimalState::Wander;
            float angle = frand() * 6.2831853f;
            a.wander_x = std::cos(angle);
            a.wander_y = std::sin(angle);
            a.state_timer = 3.0f + frand() * 2.0f;
        }
        a.vel_x = a.wander_x * t.walk_speed;
        a.vel_y = a.wander_y * t.walk_speed;
    }
    integrate_with_terrain(a, world, dt);
}

void AnimalSystem::tick_fish(AnimalInstance& a, const World& world, float dt) {
    auto t = traits_for(AnimalKind::Fish);
    a.state_timer -= dt;
    if (a.state_timer <= 0.0f) {
        float angle = frand() * 6.2831853f;
        a.wander_x = std::cos(angle);
        a.wander_y = std::sin(angle);
        a.state_timer = 1.0f + frand() * 2.0f;
    }
    a.vel_x = a.wander_x * t.walk_speed;
    a.vel_y = a.wander_y * t.walk_speed;

    // Fish only move along river/swamp cells; if they drift onto land we
    // bounce them back.
    Biome b = world.biome_at(a.pos_x + a.vel_x * dt, a.pos_y + a.vel_y * dt);
    if (b != Biome::River && b != Biome::Swamp) {
        a.vel_x = -a.vel_x;
        a.vel_y = -a.vel_y;
        a.wander_x = -a.wander_x;
        a.wander_y = -a.wander_y;
    }
    integrate_with_terrain(a, world, dt);
    a.pos_z -= 0.3f;  // sit slightly under the surface
}

void AnimalSystem::tick_eagle(AnimalInstance& a, const World& world, float dt) {
    auto t = traits_for(AnimalKind::Eagle);
    a.state_timer -= dt;
    if (a.state_timer <= 0.0f) {
        float angle = frand() * 6.2831853f;
        a.wander_x = std::cos(angle);
        a.wander_y = std::sin(angle);
        a.state_timer = 4.0f + frand() * 4.0f;
    }
    a.vel_x = a.wander_x * t.walk_speed;
    a.vel_y = a.wander_y * t.walk_speed;
    a.pos_x += a.vel_x * dt;
    a.pos_y += a.vel_y * dt;
    float wx_max = world.heightmap().world_size_x();
    float wy_max = world.heightmap().world_size_y();
    a.pos_x = std::clamp(a.pos_x, 0.0f, wx_max);
    a.pos_y = std::clamp(a.pos_y, 0.0f, wy_max);
    a.pos_z = world.surface_height(a.pos_x, a.pos_y) + 25.0f;
    a.yaw = std::atan2(a.vel_y, a.vel_x);
}

void AnimalSystem::tick(const World& world,
                        float agent_x, float agent_y, float /*agent_z*/,
                        float dt,
                        float night_factor) {
    for (auto& a : animals_) {
        if (a.state == AnimalState::Dead) continue;
        switch (a.kind) {
            case AnimalKind::Rabbit: tick_rabbit(a, world, agent_x, agent_y, dt); break;
            case AnimalKind::Deer:   tick_deer  (a, world, agent_x, agent_y, dt); break;
            case AnimalKind::Wolf:   tick_wolf  (a, world, agent_x, agent_y, dt, night_factor); break;
            case AnimalKind::Fish:   tick_fish  (a, world, dt); break;
            case AnimalKind::Eagle:  tick_eagle (a, world, dt); break;
        }
    }
}

int AnimalSystem::closest_to(float x, float y, AnimalKind kind, float max_dist) const {
    float best = max_dist * max_dist;
    int   best_idx = -1;
    for (size_t i = 0; i < animals_.size(); ++i) {
        const auto& a = animals_[i];
        if (a.kind != kind || a.state == AnimalState::Dead) continue;
        float dx = a.pos_x - x;
        float dy = a.pos_y - y;
        float d = dx * dx + dy * dy;
        if (d < best) { best = d; best_idx = static_cast<int>(i); }
    }
    return best_idx;
}

bool AnimalSystem::wolf_attack_agent(int wolf_idx, float agent_x, float agent_y,
                                     float bite_radius) const {
    if (wolf_idx < 0 || wolf_idx >= static_cast<int>(animals_.size())) return false;
    const auto& w = animals_[wolf_idx];
    if (w.kind != AnimalKind::Wolf || w.state == AnimalState::Dead) return false;
    float dx = w.pos_x - agent_x;
    float dy = w.pos_y - agent_y;
    return dx * dx + dy * dy <= bite_radius * bite_radius;
}

void AnimalSystem::kill(int idx) {
    if (idx < 0 || idx >= static_cast<int>(animals_.size())) return;
    animals_[idx].state = AnimalState::Dead;
    animals_[idx].vel_x = 0.0f;
    animals_[idx].vel_y = 0.0f;
}

}  // namespace dp::world
