#include "world/AnimalSystem.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace dp::world {

// Env-var override for hunt tuning, cached on first use so concurrent training
// processes can sweep persistence-hunt knobs without recompiling N binaries.
static float env_f(const char* name, float dflt) {
    const char* v = std::getenv(name);
    if (!v || !*v) return dflt;
    char* end = nullptr;
    float f = std::strtof(v, &end);
    return (end == v) ? dflt : f;
}

static int env_i(const char* name, int dflt) {
    const char* v = std::getenv(name);
    if (!v || !*v) return dflt;
    char* end = nullptr;
    long n = std::strtol(v, &end, 10);
    return (end == v) ? dflt : static_cast<int>(n);
}

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

    float wx_max = world.heightmap().world_size_x();
    float wy_max = world.heightmap().world_size_y();
    float span   = 0.5f * (wx_max + wy_max);

    // Spawn wildlife in social groups - rabbit warrens, deer herds, wolf
    // packs, fish schools - clustered around a biome-matched centre, instead
    // of scattering every individual independently across the whole map. Real
    // ecosystems clump (you find several deer together, then none for a long
    // stretch); clustering also means that when the agent stumbles onto prey
    // there are usually a few in reach, so a hunt attempt has more than one
    // shot at landing - the encounter rate that flat scatter starved.
    auto add = [&](AnimalKind k, int n, int herd_lo, int herd_hi,
                   float radius_frac) {
        if (n <= 0) return;
        float radius = radius_frac * span;
        int placed = 0;
        while (placed < n) {
            // A throwaway spawn finds a centre cell in this kind's preferred
            // biome (best-effort, falls back to anywhere).
            AnimalInstance centre;
            spawn(centre, k, world);
            float cx = centre.pos_x, cy = centre.pos_y;
            int size = herd_lo + static_cast<int>(frand() * (herd_hi - herd_lo + 1));
            if (size < 1) size = 1;
            if (size > n - placed) size = n - placed;
            for (int m = 0; m < size; ++m) {
                AnimalInstance a;
                spawn(a, k, world);  // sets kind/health/yaw/wander
                float ang = frand() * 6.2831853f;
                float rad = radius * std::sqrt(frand());
                a.pos_x = std::clamp(cx + std::cos(ang) * rad, 0.0f, wx_max);
                a.pos_y = std::clamp(cy + std::sin(ang) * rad, 0.0f, wy_max);
                a.pos_z = world.surface_height(a.pos_x, a.pos_y) + 0.5f;
                if (k == AnimalKind::Eagle) a.pos_z += 25.0f;
                animals_.push_back(a);
                ++placed;
            }
        }
    };
    // D-128: env-gated population override. The D-127 v-sweep proved the agent
    // reaches land prey to 0.6m yet lands ZERO of its many spear hits on
    // rabbit/deer (prey_kind_hits=0 across 180-271 eps): it fishes the
    // riverbank instead of hunting land prey. These knobs let a variant starve
    // the fishing basin (fewer/no fish) and ease predation (fewer wolves) to
    // force land-hunting, without touching the locked 582-dim obs / 23-action
    // geometry. Defaults preserve the shipped counts when unset.
    rabbits = env_i("DP_RABBIT_COUNT", rabbits);
    deer    = env_i("DP_DEER_COUNT",   deer);
    wolves  = env_i("DP_WOLF_COUNT",   wolves);
    fish    = env_i("DP_FISH_COUNT",   fish);
    add(AnimalKind::Rabbit, rabbits, 4, 7, 0.020f);  // warrens
    add(AnimalKind::Deer,   deer,    3, 6, 0.028f);  // grazing herds
    add(AnimalKind::Wolf,   wolves,  2, 3, 0.035f);  // packs
    add(AnimalKind::Fish,   fish,    5, 9, 0.014f);  // schools (in water)
    add(AnimalKind::Eagle,  eagles,  1, 1, 0.000f);  // solitary raptors
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

    // D-134 HUNT_V2 (env-gated, default OFF). Two-phase prey movement: a
    // healthy rabbit grazes at walk_speed and does NOT flee from proximity, so
    // a hunter can walk right up and land the first spear. A hit (health drops
    // below full) makes agent_attack set state=Flee/state_timer, which drives a
    // short sprint BURST here; once the timer expires the rabbit calms and
    // slow-wanders WITHOUT re-panicking, leaving a window to close and finish.
    // Each new hit refreshes the burst. Replaces the legacy persistence chase.
    static const bool HUNT_V2 = env_i("DP_HUNT_V2", 0) != 0;
    if (HUNT_V2) {
        static const float V2_REST = env_f("DP_HUNT_V2_REST", 2.5f);  // sitting-duck secs
        bool wounded = a.health < 99.0f;
        if (wounded && a.state == AnimalState::Flee && a.state_timer > 0.0f) {
            float d = std::sqrt(d2) + 1e-3f;
            a.vel_x = -dx / d * t.panic_speed;     // sprint straight away
            a.vel_y = -dy / d * t.panic_speed;
            a.state_timer -= dt;
            if (a.state_timer <= 0.0f) {           // burst spent -> drop to rest
                a.flee_rest = V2_REST;
                a.state = AnimalState::Wander;
            }
        } else if (a.flee_rest > 0.0f) {
            a.vel_x = 0.0f;                         // panting, stationary: the kill
            a.vel_y = 0.0f;                         // window -- no re-flee on approach
            a.flee_rest -= dt;
        } else {
            if (a.state != AnimalState::Wander) {
                a.state = AnimalState::Wander;
                a.state_timer = 1.5f + frand() * 1.5f;
            }
            a.state_timer -= dt;
            if (a.state_timer <= 0.0f) {
                float angle = frand() * 6.2831853f;
                a.wander_x = std::cos(angle);
                a.wander_y = std::sin(angle);
                a.state_timer = 1.5f + frand() * 1.5f;
            }
            a.vel_x = a.wander_x * t.walk_speed;
            a.vel_y = a.wander_y * t.walk_speed;
        }
        integrate_with_terrain(a, world, dt);
        return;
    }

    // Persistence hunting. A healthy rabbit is faster than the agent (6 m/s vs
    // 3) and only bolts once the agent is fairly near (FLEE_TRIGGER), sprinting
    // in bursts then panting (SPRINT_CAP/REST_DUR) so a tracker can close in.
    // Crucially, a SPEARED rabbit (health < full after the first 50-dmg blow)
    // bleeds and tires: its sprint drops below human pace and its panting runs
    // longer, so the wounded animal becomes catchable for the killing 2nd hit.
    // Healthy prey stay faster than the agent -- only the wounded slow below it.
    // D-126b: v9 proved rests now open but prey collapse ~10m out and the 2s
    // window only let the 3 m/s agent close 6m. Tire sooner (more frequent
    // rests) + pant longer (bigger catch window). Healthy sprint stays 6 m/s
    // (> agent), so the "animals faster than human" rule still holds.
    static const float STAM_MAX     = env_f("DP_RAB_STAM", 2.0f);  // sprint secs before collapse
    static const float REST_DUR     = env_f("DP_RAB_REST", 3.0f);  // secs stopped to pant
    static const float RECOVER      = env_f("DP_RAB_RECOVER", 0.4f); // stamina/sec when safe
    static const float FLEE_TRIGGER = env_f("DP_RAB_TRIGGER", 12.0f); // bolt when agent this near
    bool agent_close = d2 < FLEE_TRIGGER * FLEE_TRIGGER;
    if (agent_close) {
        a.state = AnimalState::Flee;
        float d = std::sqrt(d2) + 1e-3f;
        bool  wounded = a.health < 99.0f;             // took at least one hit
        float pspd    = wounded ? t.panic_speed * 0.42f : t.panic_speed;
        float rest    = wounded ? REST_DUR * 1.8f : REST_DUR;
        if (a.flee_rest > 0.0f) {
            // winded: stop and pant -> the pursuing agent gains ground
            a.flee_rest -= dt;
            a.vel_x = 0.0f;
            a.vel_y = 0.0f;
            if (a.flee_rest <= 0.0f) a.flee_stamina = STAM_MAX;  // caught breath
        } else {
            // healthy prey still sprint FASTER than the agent (>3 m/s); only
            // cumulative fatigue (not a per-burst timer) eventually grounds it.
            a.vel_x = -dx / d * pspd;
            a.vel_y = -dy / d * pspd;
            a.flee_stamina -= dt;
            if (a.flee_stamina <= 0.0f) a.flee_rest = rest;  // collapse -> window
        }
        a.state_timer = 1.0f;
    } else {
        // Safe: stamina refills SLOWLY (it is NOT reset), so a brief escape past
        // the trigger cannot undo the fatigue a sustained chase has built up.
        a.flee_stamina = std::min(STAM_MAX, a.flee_stamina + RECOVER * dt);
        a.flee_rest    = 0.0f;
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

    // D-134 HUNT_V2 (env-gated, default OFF): two-phase prey movement, mirror
    // of tick_rabbit -- graze at walk_speed with no proximity flee; a hit
    // triggers a short sprint burst then a calm, catchable slow-wander.
    static const bool HUNT_V2_D = env_i("DP_HUNT_V2", 0) != 0;
    if (HUNT_V2_D) {
        static const float V2_REST_D = env_f("DP_HUNT_V2_REST", 2.5f);
        bool wounded = a.health < 99.0f;
        if (wounded && a.state == AnimalState::Flee && a.state_timer > 0.0f) {
            float d = std::sqrt(d2) + 1e-3f;
            a.vel_x = -dx / d * t.panic_speed;
            a.vel_y = -dy / d * t.panic_speed;
            a.state_timer -= dt;
            if (a.state_timer <= 0.0f) {
                a.flee_rest = V2_REST_D;
                a.state = AnimalState::Wander;
            }
        } else if (a.flee_rest > 0.0f) {
            a.vel_x = 0.0f;
            a.vel_y = 0.0f;
            a.flee_rest -= dt;
        } else {
            if (a.state != AnimalState::Wander) {
                a.state = AnimalState::Wander;
                a.state_timer = 2.0f + frand() * 2.0f;
            }
            a.state_timer -= dt;
            if (a.state_timer <= 0.0f) {
                float angle = frand() * 6.2831853f;
                a.wander_x = std::cos(angle);
                a.wander_y = std::sin(angle);
                a.state_timer = 2.0f + frand() * 2.0f;
            }
            a.vel_x = a.wander_x * t.walk_speed;
            a.vel_y = a.wander_y * t.walk_speed;
        }
        integrate_with_terrain(a, world, dt);
        return;
    }

    // Persistence hunting (see tick_rabbit): a healthy deer sprints at 7 m/s and
    // bolts only when the agent is near; a speared (wounded) deer slows below
    // human pace and pants longer, becoming catchable for the killing 2nd hit.
    // D-126b: see tick_rabbit. Tire sooner + pant longer so the 3 m/s agent can
    // close during the rest window; healthy deer still sprint 7 m/s (> agent).
    static const float STAM_MAX     = env_f("DP_DEER_STAM", 2.5f);  // sprint secs before collapse
    static const float REST_DUR     = env_f("DP_DEER_REST", 3.2f);  // secs stopped to pant
    static const float RECOVER      = env_f("DP_DEER_RECOVER", 0.4f); // stamina/sec when safe
    static const float FLEE_TRIGGER = env_f("DP_DEER_TRIGGER", 16.0f); // bolt when agent this near
    bool agent_close = d2 < FLEE_TRIGGER * FLEE_TRIGGER;
    if (agent_close) {
        a.state = AnimalState::Flee;
        float d = std::sqrt(d2) + 1e-3f;
        bool  wounded = a.health < 99.0f;
        float pspd    = wounded ? t.panic_speed * 0.40f : t.panic_speed;
        float rest    = wounded ? REST_DUR * 1.8f : REST_DUR;
        if (a.flee_rest > 0.0f) {
            a.flee_rest -= dt;
            a.vel_x = 0.0f;
            a.vel_y = 0.0f;
            if (a.flee_rest <= 0.0f) a.flee_stamina = STAM_MAX;  // caught breath
        } else {
            // healthy deer sprint at 7 m/s (> agent); cumulative fatigue, not a
            // per-burst timer, is what finally grounds it for the catch.
            a.vel_x = -dx / d * pspd;
            a.vel_y = -dy / d * pspd;
            a.flee_stamina -= dt;
            if (a.flee_stamina <= 0.0f) a.flee_rest = rest;  // collapse -> window
        }
        a.state_timer = 2.0f;
    } else {
        // Safe: stamina refills SLOWLY (not reset) so brief escapes don't erase
        // the fatigue built up over a sustained chase.
        a.flee_stamina = std::min(STAM_MAX, a.flee_stamina + RECOVER * dt);
        a.flee_rest    = 0.0f;
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

void AnimalSystem::tick_wolf(AnimalInstance& a, int self_idx, const World& world,
                             float ax, float ay, float dt, float night_factor,
                             float agent_perception_scale,
                             float agent_speed_scale) {
    auto t = traits_for(AnimalKind::Wolf);
    // Nocturnal aggression: at night, wolves notice the agent up to 50%
    // farther away and chase 20% faster. night_factor is clamped [0,1].
    if (night_factor < 0.0f) night_factor = 0.0f;
    if (night_factor > 1.0f) night_factor = 1.0f;
    float perception_base = t.perception_range * (1.0f + 0.5f * night_factor);
    float speed_base      = t.panic_speed     * (1.0f + 0.2f * night_factor);
    // Shelter dampening only applies to *agent* detection, not prey.
    float perception_agent = perception_base * agent_perception_scale;
    float speed_agent      = speed_base      * agent_speed_scale;

    // Look for the closest live rabbit/deer first; that prey gets
    // priority over the agent if it's closer. Tamed (follower) prey is
    // skipped so the agent's pets don't get eaten by wild wolves.
    int   best_prey   = -1;
    float best_prey_d2 = perception_base * perception_base;
    for (int i = 0; i < int(animals_.size()); ++i) {
        if (i == self_idx) continue;
        const auto& p = animals_[i];
        if (p.state == AnimalState::Dead) continue;
        if (p.is_tamed) continue;
        if (p.kind != AnimalKind::Rabbit && p.kind != AnimalKind::Deer) continue;
        float dx = p.pos_x - a.pos_x;
        float dy = p.pos_y - a.pos_y;
        float d2 = dx*dx + dy*dy;
        if (d2 < best_prey_d2) {
            best_prey_d2 = d2;
            best_prey    = i;
        }
    }

    float dx_a = ax - a.pos_x, dy_a = ay - a.pos_y;
    float d2_a = dx_a * dx_a + dy_a * dy_a;
    bool agent_in_range = d2_a < perception_agent * perception_agent;

    // Decide: attack closer of (agent, best_prey)
    bool   chase_prey = (best_prey >= 0)
                       && (!agent_in_range || best_prey_d2 < d2_a);

    if (chase_prey) {
        const auto& p = animals_[best_prey];
        float dx = p.pos_x - a.pos_x;
        float dy = p.pos_y - a.pos_y;
        float d  = std::sqrt(dx*dx + dy*dy) + 1e-3f;
        a.state = AnimalState::Hunt;
        a.vel_x = dx / d * speed_base;
        a.vel_y = dy / d * speed_base;
        // If close enough, kill prey immediately (one-shot bite).
        if (d < 1.5f) {
            kill(best_prey);
        }
    } else if (agent_in_range) {
        a.state = AnimalState::Hunt;
        float d = std::sqrt(d2_a) + 1e-3f;
        a.vel_x = dx_a / d * speed_agent;
        a.vel_y = dy_a / d * speed_agent;
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
                        float night_factor,
                        float agent_perception_scale,
                        float agent_speed_scale) {
    for (int i = 0; i < int(animals_.size()); ++i) {
        auto& a = animals_[i];
        if (a.state == AnimalState::Dead) continue;
        switch (a.kind) {
            case AnimalKind::Rabbit: tick_rabbit(a, world, agent_x, agent_y, dt); break;
            case AnimalKind::Deer:   tick_deer  (a, world, agent_x, agent_y, dt); break;
            case AnimalKind::Wolf:   tick_wolf  (a, i, world, agent_x, agent_y, dt,
                                                 night_factor,
                                                 agent_perception_scale,
                                                 agent_speed_scale); break;
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

void AnimalSystem::mark_tamed(int idx, bool yes) {
    if (idx < 0 || idx >= int(animals_.size())) return;
    animals_[idx].is_tamed = yes;
}

void AnimalSystem::steer_toward(int idx, float tx, float ty,
                                float speed, float dt) {
    if (idx < 0 || idx >= int(animals_.size())) return;
    auto& a = animals_[idx];
    if (a.state == AnimalState::Dead) return;
    float dx = tx - a.pos_x;
    float dy = ty - a.pos_y;
    float d  = std::sqrt(dx*dx + dy*dy);
    if (d > 1.5f) {
        a.vel_x = dx / (d + 1e-3f) * speed;
        a.vel_y = dy / (d + 1e-3f) * speed;
        a.pos_x += a.vel_x * dt;
        a.pos_y += a.vel_y * dt;
        if (std::abs(a.vel_x) + std::abs(a.vel_y) > 0.01f) {
            a.yaw = std::atan2(a.vel_y, a.vel_x);
        }
    } else {
        // close enough - idle next to the player
        a.vel_x = 0.0f;
        a.vel_y = 0.0f;
    }
}

AnimalSystem::AttackResult AnimalSystem::agent_attack(
        float ax, float ay, float yaw,
        float reach, float damage) {
    AttackResult res;
    float fx = std::cos(yaw);
    float fy = std::sin(yaw);
    float reach2 = reach * reach;
    // D-127: with DP_PREY_PRIORITY=1, a spear thrust prefers the nearest
    // rabbit/deer over any closer fish/wolf in the same cone. The v-sweep
    // proved the agent reaches prey to 0m yet kills=0 because fish (schools of
    // 5-9) sit closer and eat the blow (`agent_attack` picked nearest of ANY
    // species). Prioritising prey lets the 2nd killing hit land on the wounded
    // prey instead of being wasted on a fish.
    static const bool PREY_PRIORITY = []{
        const char* v = std::getenv("DP_PREY_PRIORITY");
        return v && *v && *v != '0';
    }();
    // D-134 HUNT_V2: among prey in cone+reach, FINISH the one already wounded
    // (lowest health, tiebreak nearest) instead of grabbing the nearest fresh
    // herd member. Without this, in a herd every thrust hits a new full-health
    // animal -> 7 hits spread over 7 rabbits, none accrues the 2nd killing
    // blow (the v-sweep smoking gun: prey_kind_hits=7, kills=0). Default OFF
    // keeps the legacy "nearest prey" pick.
    static const bool HUNT_V2 = []{
        const char* v = std::getenv("DP_HUNT_V2");
        return v && *v && *v != '0';
    }();
    int   best_any_i  = -1; float best_any_d2  = reach2;
    int   best_prey_i = -1; float best_prey_d2 = reach2;
    float best_prey_hp = 1e9f;
    for (int i = 0; i < int(animals_.size()); ++i) {
        auto& a = animals_[i];
        if (a.state == AnimalState::Dead) continue;
        // Eagles are out of melee reach (~25m above ground)
        if (a.kind == AnimalKind::Eagle) continue;
        float dx = a.pos_x - ax;
        float dy = a.pos_y - ay;
        float d2 = dx*dx + dy*dy;
        if (d2 > reach2) continue;
        // Forward arc: dot(facing, toward_target) > 0 (>=90 deg cone). D-134
        // HUNT_V2 drops this 360deg at melee reach: the v-sweep showed the
        // oracle spams attacks point-blank (atk_prey_reach=488) but its facing
        // is undefined when prey sits at ~0m (atk_prey_cone=12) -> the cone
        // gate, not aim, was eating 97% of thrusts. A 3m spear jab needs no
        // precise heading.
        if (!HUNT_V2 && dx * fx + dy * fy < 0.0f) continue;
        if (d2 < best_any_d2) { best_any_d2 = d2; best_any_i = i; }
        bool is_prey = (a.kind == AnimalKind::Rabbit || a.kind == AnimalKind::Deer);
        if (is_prey) {
            if (HUNT_V2) {
                // wounded-first: lower health wins; near-equal health -> nearer.
                if (best_prey_i < 0 || a.health < best_prey_hp - 0.5f
                    || (std::abs(a.health - best_prey_hp) <= 0.5f && d2 < best_prey_d2)) {
                    best_prey_i = i; best_prey_d2 = d2; best_prey_hp = a.health;
                }
            } else if (d2 < best_prey_d2) {
                best_prey_d2 = d2; best_prey_i = i;
            }
        }
    }
    res.index = (PREY_PRIORITY && best_prey_i >= 0) ? best_prey_i : best_any_i;
    if (res.index < 0) return res;
    auto& tgt = animals_[res.index];
    static const bool V2HIT_DEBUG = []{
        const char* v = std::getenv("DP_V2HIT_DEBUG");
        return v && *v && *v != '0';
    }();
    if (V2HIT_DEBUG && HUNT_V2 &&
        (tgt.kind == AnimalKind::Rabbit || tgt.kind == AnimalKind::Deer)) {
        std::fprintf(stderr, "[V2HIT] idx=%d hp_pre=%.0f dmg=%.0f d=%.2f best_prey_hp=%.0f preyfirst=%d\n",
                     res.index, tgt.health, damage, std::sqrt(best_any_d2),
                     best_prey_hp, (PREY_PRIORITY && best_prey_i >= 0) ? 1 : 0);
    }
    tgt.health -= damage;
    if (tgt.health <= 0.0f) {
        tgt.health = 0.0f;
        tgt.state = AnimalState::Dead;
        tgt.vel_x = 0.0f;
        tgt.vel_y = 0.0f;
        res.killed = true;
    } else {
        // Hurt animal flees. D-134 HUNT_V2: keep the burst SHORT (default
        // 0.35s) so the wounded prey only opens ~2m before dropping to its
        // stationary rest -- a 1.5s/6m-s burst (legacy) flung it ~9m away,
        // out of reach, and the hunter (targeting nearest) abandoned it for a
        // fresh herd member, spreading hits and never landing the 2nd blow
        // (opp_rest=0, kills=0 across the v-sweep). A short burst leaves the
        // wounded animal resting within reach for the finishing thrust.
        static const float V2_BURST = []{
            const char* v = std::getenv("DP_HUNT_V2_BURST");
            return (v && *v) ? float(std::atof(v)) : 0.35f;
        }();
        tgt.state = AnimalState::Flee;
        tgt.state_timer = HUNT_V2 ? V2_BURST : 1.5f;
    }
    return res;
}

}  // namespace dp::world
