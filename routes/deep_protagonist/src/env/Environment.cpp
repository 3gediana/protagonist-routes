#include "env/Environment.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace dp::env {

namespace {

// Cheap deterministic 0..99 from three uint64s. Matches the inline RNG
// used in main.cpp for mushroom poison + seed drop chance, so behaviour
// is identical between viewer and headless mode.
unsigned hash01(uint64_t a, uint64_t b, uint64_t c) {
    uint64_t r = a * 7919ULL ^ b * 31337ULL ^ c;
    return static_cast<unsigned>(r);
}

}  // namespace

Environment::Environment(const EnvConfig& cfg)
    : cfg_(cfg),
      world_(cfg.world),
      day_(600.0f, 0.30f),
      agent_(cfg.world.seed ^ cfg.instance_seed_xor),
      vitals_([&]() {
          dp::agent::VitalConfig vc;
          vc.thirst_decay_per_min = cfg.thirst_decay_per_min;
          return vc;
      }())
{
    // Match main.cpp's deterministic init order so we get the exact same
    // world layout for given seed.
    decorations_.generate(cfg_.world.seed ^ 0xDEC0ULL, world_);
    plants_.initialize(cfg_.world.seed     ^ 0xF00DULL, world_, decorations_);
    animals_.initialize(cfg_.world.seed    ^ 0xA11AULL, world_);
    resources_.initialize(cfg_.world.seed  ^ 0xC011ULL, world_);

    float wx_max = world_.heightmap().world_size_x();
    float wy_max = world_.heightmap().world_size_y();

    // D-050: blueprint line 9-10 demands "楗夸簡鎵鹃鐗╋紝娓翠簡鎵炬按" be
    // *learnable*. Pre D-050 spawn was hard-pinned at the world centre
    // (512, 512); for the default 1024 m world with 3 procedural rivers
    // the centre often sits >60 m from any River|Swamp cell, which means
    // the agent's nearest_water obs is "no water in sight" from frame 0
    // and the only way to drink is to randomly walk into a river. PPO
    // never learns the link "Q on river -> r_water > 0" because the
    // first such event hardly ever happens in 9000 steps. (D-049's
    // SpatialMemory vision-write fixes "see -> remember", but only after
    // the agent has been in vision range of water at least once.)
    //
    // Now we choose a spawn within the central region but require that
    // a River or Swamp cell exists inside VISION_RANGE = 60 m. Search:
    //   1. try the centre first (cheap, reproduces old layout when there
    //      is already water nearby);
    //   2. if dry, spiral outwards in 32 m steps over 16 angular slots
    //      until a candidate is found;
    //   3. fall back to centre if the world has no water at all (should
    //      never trigger with the default 3-river config).
    auto biome_at = [&](float x, float y) {
        if (x < 0.0f || y < 0.0f || x >= wx_max || y >= wy_max) {
            return dp::world::Biome::Plain;
        }
        return world_.biome_at(x, y);
    };
    auto has_water_in_vision = [&](float cx, float cy) {
        const float R = dp::agent::ObservationBuilder::VISION_RANGE;
        const float step = 8.0f;
        const float R2 = R * R;
        for (float dy = -R; dy <= R; dy += step) {
            for (float dx = -R; dx <= R; dx += step) {
                if (dx*dx + dy*dy > R2) continue;
                auto b = biome_at(cx + dx, cy + dy);
                if (b == dp::world::Biome::River
                 || b == dp::world::Biome::Swamp) {
                    return true;
                }
            }
        }
        return false;
    };
    auto in_dry_land = [&](float x, float y) {
        auto b = biome_at(x, y);
        return b != dp::world::Biome::River
            && b != dp::world::Biome::Swamp;
    };
    // D-050 P3: also require ripe food within vision range. The first
    // P2 spawn (632.5, 214.4) had water in sight but zero plants nearby
    // in the wander 100k smoke -> food=0 across the whole episode, so
    // PPO never sampled a successful eat() either. Blueprint line 21
    // bundles "楗夸簡鎵鹃鐗╋紝娓翠簡鎵炬按" as one early-stage loop, so the
    // spawn must satisfy *both* preconditions to be learnable.
    auto food_in_vision = [&](float cx, float cy) {
        const float R = dp::agent::ObservationBuilder::VISION_RANGE;
        const float R2 = R * R;
        for (const auto& pl : plants_.plants()) {
            if (!pl.ripe) continue;
            float dx = pl.x - cx, dy = pl.y - cy;
            if (dx*dx + dy*dy <= R2) return true;
        }
        return false;
    };

    spawn_x_ = wx_max * 0.5f;
    spawn_y_ = wy_max * 0.5f;
    auto candidate_ok = [&](float x, float y) {
        return has_water_in_vision(x, y)
            && food_in_vision(x, y)
            && in_dry_land(x, y);
    };
    if (!candidate_ok(spawn_x_, spawn_y_)) {
        bool found = false;
        for (float r = 32.0f; r <= 400.0f && !found; r += 32.0f) {
            for (int a = 0; a < 24 && !found; ++a) {
                float ang = a * 6.2831853f / 24.0f;
                float cx = wx_max * 0.5f + r * std::cos(ang);
                float cy = wy_max * 0.5f + r * std::sin(ang);
                if (cx < 16.0f || cy < 16.0f
                 || cx >= wx_max - 16.0f
                 || cy >= wy_max - 16.0f) continue;
                if (candidate_ok(cx, cy)) {
                    spawn_x_ = cx;
                    spawn_y_ = cy;
                    found = true;
                }
            }
        }
        if (!found) {
            // Relax: water-only fallback (still better than the old
            // dead-centre spawn).
            for (float r = 32.0f; r <= 400.0f && !found; r += 32.0f) {
                for (int a = 0; a < 24 && !found; ++a) {
                    float ang = a * 6.2831853f / 24.0f;
                    float cx = wx_max * 0.5f + r * std::cos(ang);
                    float cy = wy_max * 0.5f + r * std::sin(ang);
                    if (cx < 16.0f || cy < 16.0f
                     || cx >= wx_max - 16.0f
                     || cy >= wy_max - 16.0f) continue;
                    if (has_water_in_vision(cx, cy)
                     && in_dry_land(cx, cy)) {
                        spawn_x_ = cx;
                        spawn_y_ = cy;
                        found = true;
                    }
                }
            }
            spdlog::warn("Environment D-050: no spawn satisfies "
                         "water+food within 400 m - relaxed to water-only");
        }
    }
    spawn_z_ = world_.surface_height(spawn_x_, spawn_y_) + 5.0f;
    spdlog::info("Environment D-050: spawn=({:.1f},{:.1f},{:.1f}), "
                 "water_in_vision={} food_in_vision={}",
                 spawn_x_, spawn_y_, spawn_z_,
                 has_water_in_vision(spawn_x_, spawn_y_) ? "yes" : "no",
                 food_in_vision(spawn_x_, spawn_y_) ? "yes" : "no");

    // Debug: closest plant by kind
    for (auto kind : {dp::world::PlantKind::Berry,
                      dp::world::PlantKind::Fruit}) {
        float best_d = 1e9f, best_x = 0, best_y = 0;
        for (const auto& p : plants_.plants()) {
            if (p.kind != kind) continue;
            float dx = p.x - spawn_x_, dy = p.y - spawn_y_;
            float d  = std::sqrt(dx*dx + dy*dy);
            if (d < best_d) { best_d = d; best_x = p.x; best_y = p.y; }
        }
        const char* knm = (kind == dp::world::PlantKind::Berry)
                          ? "berry" : "fruit";
        if (best_d < 1e9f) {
            spdlog::info("Environment D-050: closest {} at ({:.1f},{:.1f}) "
                         "dist={:.1f} m",
                         knm, best_x, best_y, best_d);
        }
    }

    dp::agent::EpisodeManager::Config ecfg;
    ecfg.spawn_x          = spawn_x_;
    ecfg.spawn_y          = spawn_y_;
    ecfg.spawn_clearance  = 5.0f;
    ecfg.max_seconds      = cfg_.max_episode_seconds;
    ecfg.animal_seed_base = cfg_.world.seed ^ 0xA11AULL;
    episodes_.configure(ecfg);

    spatial_.configure(0.0f, 0.0f);
}

void Environment::reset(dp::agent::ObservationBuilder::Vector& obs_out) {
    // First-time reset (episode_id == -1) or post-done reset both go
    // through here.
    if (episodes_.episode_id() < 0) {
        agent_.reset(spawn_x_, spawn_y_, spawn_z_);
        // Initialise vital state to full / neutral.
        vital_state_ = dp::agent::VitalState{};
        episodes_.begin_episode();
    } else {
        episodes_.reset(world_, agent_, vitals_, vital_state_,
                        reward_, animals_, inventory_, shelter_,
                        buildings_);
        wardrobe_.clear();
        farming_.clear();
        tame_.clear();
    }

    was_alive_  = true;
    step_index_ = 0;
    selected_building_ = dp::world::BuildingType::Shed;
    last_seed_kind_    = dp::world::PlantKind::Berry;
    spatial_.clear();   // cognitive map is per-life (one episode)

    // D-041: reset all first_X milestone flags PER episode (not per
    // training run as before). This is the core fix for v6's "milestone
    // mean=0" plateau: the agent had used up all milestones in episode 0
    // and had no carrot to chase in subsequent episodes. Per-episode
    // milestones turn the main line into a recurring +20*N reward signal
    // worth competing with drink density.
    first_drink_fired_          = false;
    first_eat_fired_            = false;
    first_collect_fired_        = false;
    first_spear_fired_          = false;
    first_shelter_fired_        = false;
    first_clothing_fired_       = false;
    first_hunt_fired_           = false;
    first_seed_planted_fired_   = false;
    first_house_built_fired_    = false;
    first_crop_harvested_fired_ = false;
    first_follower_fired_       = false;

    // D-063: starter kit. D-062 proved +5 critical alone can't make
    // PPO explore the craft_spear -> hunt path in 2M steps (0% first_spear,
    // 0% first_hunt in last 60 eps). Pre-place Wood+Stone so craft_spear
    // succeeds on first attempt -> +5 critical fires -> agent learns the
    // 'tool gives bonus' association, then with spear in hand attacks on
    // rabbit/deer/fish (1-2 hits @ 50 dmg) become viable to discover.
    // Third slot left empty for PPO to fill (grass/fur).
    inventory_.add(dp::agent::ItemKind::Wood);
    inventory_.add(dp::agent::ItemKind::Stone);
    // D-065: add Grass to starter kit. D-063 proved structural change
    // (pre-place Wood+Stone) unlocked craft_spear path -> 100% spear, 8% hunt
    // without any new shaped reward. Same philosophy for line-23 clothing:
    // pre-place Grass so first craft_grass_dress works step 0 -> +5 critical
    // for first_clothing fires -> PPO associates "craft = good" for clothes,
    // analogous to spear. No per-event reward, no metric chasing.
    inventory_.add(dp::agent::ItemKind::Grass);

    rebuild_observation_(obs_out);
}

// D-043 P1: drive function (Keramati & Gutkin 2014, Homeostatic RL).
// Maps a vital reading in [0, 100] to a drive 鈭?[0, 1]. v=100 (sated) 鈫?0,
// v=0 (depleted) 鈫?1. Quadratic so deviations near the setpoint matter less
// than approaching depletion (matches biological appetite).
//   drive(100) = 0.00   (no need)
//   drive( 80) = 0.04   (mild need; eating gives small reward)
//   drive( 50) = 0.25   (moderate)
//   drive( 20) = 0.64   (strong)
//   drive(  0) = 1.00   (about to die)
// Reward = drive_before - drive_after, so eating to fullness gives a small
// reward bump near 100 and a big one near 0. Auto-replaces the old hard-cap
// satiation_scale_().
static float drive_(float vital) {
    const float gap = (100.0f - std::clamp(vital, 0.0f, 100.0f)) / 100.0f;
    return gap * gap;
}

// D-043 P1: crisis scale (Keramati's "irrelevant drive inhibition"). When
// any vital is in dangerous territory, milestone rewards are damped so the
// agent prioritizes survival over progression. Three regimes:
//   max_drive > 0.7  (any vital < ~17/100): crisis -> 0.3
//   max_drive > 0.4  (any vital < ~37/100): alert  -> 0.7
//   otherwise                              : healthy -> 1.0
// Returns a scalar in [0, 1] passed to RewardSystem::event_milestone.
static float crisis_scale_(const dp::agent::VitalState& v) {
    float max_drive = drive_(v.hunger);
    max_drive = std::max(max_drive, drive_(v.thirst));
    max_drive = std::max(max_drive, drive_(v.stamina));
    // Temperature is centered at 50; map deviation to drive-like scale.
    const float temp_dev = std::abs(v.temperature - 50.0f) / 50.0f;
    max_drive = std::max(max_drive, temp_dev * temp_dev);
    if (max_drive > 0.7f) return 0.3f;
    if (max_drive > 0.4f) return 0.7f;
    return 1.0f;
}

float Environment::apply_action_effects_(const dp::agent::AgentAction& action) {
    const auto& as = agent_.state();
    const float dt = cfg_.fixed_dt;

    reward_.begin_step();
    reward_.event_alive(dt);

    // D-041 / D-043: fire a per-episode milestone exactly once. The flag
    // resets every episode (see Environment::reset). D-043 P1: crisis_scale_
    // damps the +1 milestone when the agent is in vital crisis so it doesn't
    // grab milestones while about to die of thirst.
    const float crisis = crisis_scale_(vital_state_);
    auto fire_once = [&](bool& flag) {
        if (!flag) {
            flag = true;
            reward_.event_milestone(crisis);
        }
    };
    // D-051: a single successful first_drink / first_eat is the moment
    // the agent breaks out of "stand-still NOOP" and proves it can ride
    // the find-water / find-food loop. Stack five milestones on top of
    // the regular fire so the bonus is +5 instead of +1 the very first
    // time the agent succeeds in an episode. Crisis scaling still
    // applies (so a starving agent grabbing food gets the full reward,
    // a fed agent grinding doesn't). Subsequent eats / drinks within
    // the same episode follow the normal drive-reduction reward path.
    auto fire_once_critical = [&](bool& flag) {
        bool first = !flag;
        if (first) {
            flag = true;
            for (int i = 0; i < 5; ++i) {
                reward_.event_milestone(crisis);
            }
        }
    };

    // D-043 P1: eat/drink with drive-reduction reward (Keramati 2014).
    // Replaces the old satiation_scale_ 脳 kcal scheme. drive(v=100)=0 so
    // eating-when-full naturally yields zero reward; near-empty agents see
    // a big drive_red. Returns the *actual* vital restored so callers can
    // still detect "did the consumption happen" for first_X triggers.
    auto eat_drive = [&](float kcal) -> float {
        if (kcal <= 0.0f) return 0.0f;
        const float drive_before = drive_(vital_state_.hunger);
        const float ate          = vitals_.eat(vital_state_, kcal);
        const float drive_after  = drive_(vital_state_.hunger);
        reward_.event_eat(drive_before - drive_after);
        return ate;
    };
    auto drink_drive = [&](float ml) -> float {
        if (ml <= 0.0f) return 0.0f;
        const float drive_before = drive_(vital_state_.thirst);
        const float drunk        = vitals_.drink(vital_state_, ml);
        const float drive_after  = drive_(vital_state_.thirst);
        reward_.event_drink(drive_before - drive_after);
        return drunk;
    };

    // D-039 v3: any discrete trigger pressed this tick pays a small fee.
    // Breaks "cheap noop trigger" local optima where the policy collapses
    // to e.g. always-pressing craft_spear (it's free when inventory is
    // empty). NOOP (no trigger) avoids the fee, so the agent has to learn
    // *when* to press each key. Fee << real reward signals (drink ~1.8/s,
    // milestone +20) so successful triggers still net strongly positive.
    if (action.eat || action.drink || action.attack || action.collect
     || action.place_shelter || action.craft_spear || action.sleep
     || action.craft_grass_dress || action.craft_fur_cloak
     || action.wear_clothes || action.place_blueprint
     || action.cycle_building_type || action.deposit_to_site
     || action.plant_seed || action.water_plot || action.feed_tame) {
        reward_.event_action_cost(cfg_.trigger_cost);
    }

    // D-048 blueprint鍥炲綊: sleep is no longer a free NOOP. Pressing sleep
    // burns extra hunger/thirst (浣犵潯瑙夌殑鏃跺€欎笉浼氬枬姘翠篃涓嶄細鍚冮キ) and
    // does not regenerate stamina (the trigger is a fiction in this MVP -
    // the agent doesn't actually pause time). Without this, ppo_d046_2M
    // collapsed to "sleep 30%" as the dominant action because it was
    // costless and never triggered death (钃濆浘 line 28 makes clear sleep
    // is supposed to be 'find a shelter from the night', not idle).
    if (action.sleep) {
        const float burn = 4.0f * dt;  // 4 vital points per second of sleep
        vital_state_.hunger = std::max(0.0f, vital_state_.hunger - burn);
        vital_state_.thirst = std::max(0.0f, vital_state_.thirst - burn);
    }

    // Progression: every metre travelled while alive counts.
    float step_speed = std::sqrt(as.vel_x * as.vel_x + as.vel_y * as.vel_y);
    progress_.on_explore_step(step_speed * dt);

    // Animals + shelter dampening (mirrors main.cpp).
    // D-049 #1: blueprint line 26 says "澶╅粦鎵炬礊绌?鏋楀湴韬?. Pre D-049
    // inside_cave_ was a render-only flag (color tint + don't-fly-out
    // physics). Now caves count as a real shelter: wolves perceive the
    // agent at 30% range and chase at 70% speed when underground, and
    // body temperature trends back to neutral. This makes "run into the
    // cave at night" a learnable strategy instead of a dead UI element.
    float night_factor = std::max(0.0f, -day_.sun_height_normalized());
    float perc_scale = 1.0f, spd_scale = 1.0f;
    bool  sheltered = shelter_.covers(as.pos_x, as.pos_y)
                   || buildings_.covers(as.pos_x, as.pos_y)
                   || agent_.inside_cave();
    if (sheltered) {
        perc_scale = 0.3f;
        spd_scale  = 0.7f;
    }
    animals_.tick(world_, as.pos_x, as.pos_y, as.pos_z,
                  dt, night_factor, perc_scale, spd_scale);
    if (progress_.can_tame()) {
        tame_.apply_follow(animals_, as.pos_x, as.pos_y, dt);
    }

    plants_.tick(dt);
    resources_.tick(dt);
    farming_.tick(dt);

    float speed = std::sqrt(as.vel_x * as.vel_x + as.vel_y * as.vel_y);
    vitals_.tick(vital_state_, world_, as.pos_x, as.pos_y, speed, day_,
                 dt, wardrobe_.cold_resist());
    if (sheltered) {
        vital_state_.temperature += (50.0f - vital_state_.temperature)
                                  * 0.3f * dt;
    }
    reward_.event_vital_pressure(vital_state_, dt);

    // E -> action.eat
    if (action.eat) {
        auto got = plants_.try_harvest(as.pos_x, as.pos_y,
                                       as.pos_z + as.height * 0.5f, 2.5f);
        if (progress_.can_farm()) {
            auto farm_got = farming_.harvest_at(as.pos_x, as.pos_y, 2.0f);
            for (auto k : farm_got) got.push_back(k);
            if (!farm_got.empty() && !first_crop_harvested_fired_) {
                reward_.event_milestone();
                first_crop_harvested_fired_ = true;
            }
        }
        for (auto k : got) {
            float kcal = 0.0f, ml = 0.0f;
            switch (k) {
                case dp::world::PlantKind::Berry:    kcal = 12.0f; break;
                case dp::world::PlantKind::Fruit:    kcal = 25.0f; break;
                case dp::world::PlantKind::Cactus:   kcal =  8.0f; ml = 30.0f; break;
                case dp::world::PlantKind::Mushroom: kcal = 18.0f; break;
            }
            bool poisoned = false;
            if (k == dp::world::PlantKind::Mushroom) {
                unsigned r = hash01(
                    static_cast<uint64_t>(as.pos_x * 1000.0f),
                    static_cast<uint64_t>(step_index_ + 1),
                    static_cast<uint64_t>(episodes_.episode_id() + 1));
                if ((r % 100u) < 30u) poisoned = true;
            }
            if (poisoned) {
                vital_state_.hunger  = std::max(0.0f, vital_state_.hunger  - 12.0f);
                vital_state_.stamina = std::max(0.0f, vital_state_.stamina - 25.0f);
                // D-043: poison = inverse drive reduction (eat made things
                // worse). -0.2 drive 鈮?-0.6 reward at food_coef=3.0,
                // similar magnitude to old -15kcal 脳 0.04 = -0.6.
                reward_.event_eat(-0.2f);
            } else {
                const float ate   = eat_drive(kcal);
                if (ate > 0.5f) fire_once_critical(first_eat_fired_);
                const float drunk = drink_drive(ml);
                if (drunk > 0.5f) fire_once_critical(first_drink_fired_);
            }
            // 30% chance Fruit/Berry drops a Seed for late-stage farming.
            if (k == dp::world::PlantKind::Fruit
                || k == dp::world::PlantKind::Berry) {
                unsigned r = hash01(
                    static_cast<uint64_t>(as.pos_x * 991.0f),
                    static_cast<uint64_t>(step_index_ + 7),
                    static_cast<uint64_t>(episodes_.episode_id() + 13));
                if ((r % 100u) < 30u) {
                    inventory_.add(dp::agent::ItemKind::Seed);
                    last_seed_kind_ = k;
                }
            }
        }
    }

    // Q -> action.drink (+ try fish)
    // D-050 P6: drink fires if the agent is ON or *next to* a River|Swamp
    // cell (within 2 m). The original gate required biome_at(agent) to be
    // exactly water; in practice that meant the agent had to physically
    // step into the river - which CapsuleAgent's gravity makes unreliable
    // and which the blueprint never demanded ("娓翠簡鎵炬按" is "find water",
    // not "swim"). The looser gate matches "stand at the bank, drink".
    if (action.drink) {
        auto biome_here = world_.biome_at(as.pos_x, as.pos_y);
        bool on_water = (biome_here == dp::world::Biome::River
                      || biome_here == dp::world::Biome::Swamp);
        bool near_water = on_water;
        if (!on_water) {
            const float r = 2.0f;
            for (float dy = -r; dy <= r && !near_water; dy += 1.0f) {
                for (float dx = -r; dx <= r && !near_water; dx += 1.0f) {
                    auto nb = world_.biome_at(as.pos_x + dx,
                                              as.pos_y + dy);
                    if (nb == dp::world::Biome::River
                     || nb == dp::world::Biome::Swamp) {
                        near_water = true;
                    }
                }
            }
        }
        if (near_water) {
            const float drunk = drink_drive(35.0f);
            if (drunk > 0.5f) fire_once_critical(first_drink_fired_);
            auto fish = animals_.agent_attack(as.pos_x, as.pos_y,
                                              agent_.yaw(), 2.0f, 100.0f);
            if (fish.killed
                && animals_.animals()[fish.index].kind
                    == dp::world::AnimalKind::Fish) {
                const float ate = eat_drive(20.0f);
                if (ate > 0.5f) fire_once_critical(first_eat_fired_);
                // D-043: first_hunt removed (Crafter: no reward for attacking
                // neutral animals). Fish-while-drinking still gives food via
                // first_eat, which is the legitimate signal.
            }
        }
    }

    // R -> deposit_to_site (priority) or collect (fallback)
    if (action.deposit_to_site || action.collect) {
        bool deposited = false;
        if (action.deposit_to_site && progress_.can_build()) {
            for (int slot = 0; slot < inventory_.size(); ++slot) {
                auto k = inventory_.at(slot);
                if (k != dp::agent::ItemKind::Wood
                 && k != dp::agent::ItemKind::Stone
                 && k != dp::agent::ItemKind::Grass) continue;
                int site_idx = buildings_.try_deposit(as.pos_x, as.pos_y,
                                                     as.pos_z, 2.5f, k);
                if (site_idx >= 0) {
                    inventory_.remove_one(k);
                    // D-055: deposit gets its own bigger reward (+1.0 vs
                    // collect +0.5) so PPO can finally see the gradient
                    // from "wander around picking stuff up" to "carry it
                    // back to the construction site".
                    reward_.event_deposited_material();
                    // D-057's deposit milestone shaping reverted in D-059:
                    // any reward landscape change on top of D-055 ckpt
                    // collapsed PPO. Only the world simplification (Shed
                    // recipe down to 1 wood + 1 grass) survives.
                    fire_once(first_collect_fired_);
                    deposited = true;
                    break;
                }
            }
        }
        if (!deposited && action.collect) {
            auto picked = resources_.try_collect(as.pos_x, as.pos_y,
                                                 as.pos_z + as.height * 0.5f,
                                                 2.0f);
            for (auto rk : picked) {
                if (inventory_.add(dp::agent::item_from_resource(rk))) {
                    reward_.event_collected_resource();
                    progress_.on_collect();
                    fire_once(first_collect_fired_);
                }
            }
        }
    }

    // P -> place_blueprint
    // D-055: a failed place_blueprint (stage-locked, or another site is
    // already in progress per the new single-site rule) now pays an extra
    // cost on top of the normal trigger_cost. D-053 eval showed PPO mashing
    // place_blueprint at 14.3% of all triggers (981 over 33 ep) because the
    // first 30 sites were free and the 31st-981st were near-free. The extra
    // failure cost gives gradient feedback "this didn't do anything".
    if (action.place_blueprint) {
        int site_id = buildings_.place_site(selected_building_,
                                            as.pos_x, as.pos_y, as.pos_z,
                                            progress_);
        if (site_id < 0) {
            reward_.event_action_cost(0.05f);
        }
    }

    // Tab -> cycle_building_type
    if (action.cycle_building_type) {
        int t = static_cast<int>(selected_building_);
        t = (t + 1) % 4;
        selected_building_ = static_cast<dp::world::BuildingType>(t);
    }

    // Promote any sites that just got their last material this tick.
    int finished = buildings_.tick_finalize();
    for (int i = 0; i < finished; ++i) {
        reward_.event_built_shelter();
        progress_.on_house_built();
        // D-059: first_house back to single milestone fire (D-056's 脳5
        // critical was reverted - any reward change on top of D-055 ckpt
        // collapsed PPO). The +5 built_shelter and +1 milestone are kept;
        // PPO needs to see this combo at least once before it converges,
        // which is now within reach because Shed recipe is only 2 deposits.
        if (!first_house_built_fired_) {
            reward_.event_milestone();
            first_house_built_fired_ = true;
        }
    }

    // Stage unlock bonus (Early->Mid, Mid->Late). +40 each (milestone x2).
    if (progress_.poll_unlock()) {
        reward_.event_milestone();
        reward_.event_milestone();
    }

    if (action.craft_spear) {
        if (inventory_.craft_spear()) {
            reward_.event_crafted_tool();
            // D-062: upgrade to critical (+5). Crafting the first spear is
            // the key gateway to line 22 hunting; making the first one
            // worth more than a routine craft helps PPO discover the
            // wood+stone -> spear pipeline. Subsequent spears within the
            // same episode follow the normal crafted_tool path.
            fire_once_critical(first_spear_fired_);
        }
    }
    if (action.place_shelter) {
        if (shelter_.try_place(inventory_, as.pos_x, as.pos_y, as.pos_z)) {
            reward_.event_built_shelter();
            fire_once(first_shelter_fired_);
        }
    }
    if (action.attack) {
        bool has_spear = inventory_.num_of(dp::agent::ItemKind::Spear) > 0;
        float reach  = has_spear ? 2.5f : 1.5f;
        float damage = has_spear ? 50.0f : 20.0f;
        auto hit = animals_.agent_attack(as.pos_x, as.pos_y,
                                         agent_.yaw(), reach, damage);
        // D-065: D-064 per-hit reward reverted. D-064 measured: hunt 8%->0%,
        // col 68%->37%, drink 100%->90%, score 0.292->0.281. Confirmed user
        // warning that direct per-hit shaping causes reward hacking. The
        // kill-time first_hunt critical (+5) below stays; we rely on
        // structural changes (D-063 starter kit + D-065 grass) to lower
        // exploration cost without metric-driven shaped reward.
        if (hit.killed) {
            auto kind = animals_.animals()[hit.index].kind;
            if (kind == dp::world::AnimalKind::Wolf) {
                // Wolf: wolf_killed stays 0 (D-047 anti-鎾╃嫾). Eat reward
                // only if hungry; NO first_hunt milestone for wolves -
                // we don't want PPO to gamble against predators for the
                // line-22 unlock.
                reward_.event_wolf_killed();
                const float ate = eat_drive(40.0f);
                if (ate > 0.5f) fire_once_critical(first_eat_fired_);
                inventory_.add(dp::agent::ItemKind::Fur);
                reward_.event_collected_resource();  // D-062: fur is a resource
            } else if (kind == dp::world::AnimalKind::Rabbit) {
                const float ate = eat_drive(15.0f);
                if (ate > 0.5f) fire_once_critical(first_eat_fired_);
                inventory_.add(dp::agent::ItemKind::Fur);
                reward_.event_collected_resource();  // D-062
                fire_once_critical(first_hunt_fired_);  // D-062
            } else if (kind == dp::world::AnimalKind::Deer) {
                const float ate = eat_drive(40.0f);
                if (ate > 0.5f) fire_once_critical(first_eat_fired_);
                inventory_.add(dp::agent::ItemKind::Fur);
                reward_.event_collected_resource();  // D-062
                fire_once_critical(first_hunt_fired_);  // D-062
            } else if (kind == dp::world::AnimalKind::Fish) {
                const float ate = eat_drive(20.0f);
                if (ate > 0.5f) fire_once_critical(first_eat_fired_);
                fire_once_critical(first_hunt_fired_);  // D-062
            }
            // D-043: first_hunt removed. D-062: re-introduced but ONLY for
            // prey (rabbit/deer/fish). Wolves still grant no kill bonus so
            // the 鎾╃嫾 pathology cannot re-emerge.
        }
    }

    if (action.craft_grass_dress) {
        if (inventory_.craft_grass_dress()) reward_.event_crafted_tool();
    }
    if (action.craft_fur_cloak) {
        if (inventory_.craft_fur_cloak()) reward_.event_crafted_tool();
    }
    if (action.wear_clothes) {
        bool worn = false;
        if (inventory_.num_of(dp::agent::ItemKind::FurCloak) > 0) {
            wardrobe_.wear(inventory_, dp::agent::ItemKind::FurCloak);
            worn = true;
        } else if (inventory_.num_of(dp::agent::ItemKind::GrassDress) > 0) {
            wardrobe_.wear(inventory_, dp::agent::ItemKind::GrassDress);
            worn = true;
        }
        // D-062: upgrade to critical (+5). First time the agent puts on
        // any garment is the line-23 unlock; make it salient enough that
        // PPO can connect craft_grass_dress/fur_cloak -> wear_clothes.
        if (worn) fire_once_critical(first_clothing_fired_);
    }

    if (action.plant_seed && progress_.can_farm()) {
        if (inventory_.num_of(dp::agent::ItemKind::Seed) > 0) {
            inventory_.remove_one(dp::agent::ItemKind::Seed);
            farming_.plant_seed(as.pos_x, as.pos_y, as.pos_z, last_seed_kind_);
            reward_.event_collected_resource();
            fire_once(first_seed_planted_fired_);
        }
    }
    if (action.water_plot && progress_.can_farm()) {
        if (farming_.water_at(as.pos_x, as.pos_y, 2.0f)) {
            vital_state_.stamina = std::max(0.0f, vital_state_.stamina - 2.0f);
        }
    }
    if (action.feed_tame && progress_.can_tame()) {
        auto fr = tame_.try_feed(animals_, inventory_,
                                 as.pos_x, as.pos_y, 3.0f);
        if (fr.animal_idx >= 0) {
            inventory_.remove_one(fr.consumed);
            reward_.event_collected_resource();
            if (fr.just_became_follower) {
                reward_.event_crafted_tool();
                if (!first_follower_fired_) {
                    reward_.event_milestone();
                    first_follower_fired_ = true;
                }
            }
        }
    }

    // Wolves can bite the agent (passive damage)
    for (size_t i = 0; i < animals_.animals().size(); ++i) {
        if (animals_.wolf_attack_agent(static_cast<int>(i),
                                       as.pos_x, as.pos_y)) {
            vital_state_.hunger  = std::max(0.0f,
                vital_state_.hunger  - 25.0f * dt);
            vital_state_.stamina = std::max(0.0f,
                vital_state_.stamina - 50.0f * dt);
            reward_.event_wolf_bite(25.0f * dt);
        }
    }

    // Death edge
    if (was_alive_ && !vital_state_.alive) {
        reward_.event_death();
    }
    was_alive_ = vital_state_.alive;

    // D-043 P2: per-step state-based bonus (桅-style "what the agent has
    // accumulated"). Awards a small ongoing payout for holding house +
    // clothes + followers + inventory diversity, so the agent learns these
    // as persistent values rather than one-shot first_X events.
    {
        float phi = 0.0f;
        // has at least one completed building: +0.5
        if (!buildings_.buildings().empty()) phi += 0.5f;
        // wearing any clothing: +0.3
        if (wardrobe_.worn() != dp::agent::ItemKind::Count) phi += 0.3f;
        // followers: +0.1 each, capped at 5 followers (0.5)
        phi += std::min(0.5f, 0.1f * static_cast<float>(tame_.follower_count()));
        // inventory diversity: +0.05 per unique kind, capped at 10 unique (0.5)
        int unique = 0;
        for (int k = 0; k < static_cast<int>(dp::agent::ItemKind::Count); ++k) {
            if (inventory_.num_of(static_cast<dp::agent::ItemKind>(k)) > 0) {
                ++unique;
            }
        }
        phi += std::min(0.5f, 0.05f * static_cast<float>(unique));
        reward_.event_state_bonus(phi);
    }

    // D-044: return the CLIPPED per-step total. Pre D-044 the caller used
    // reward_.last_step().total() which is the UNCLIPPED component sum,
    // so the [-10,+10] safety clip declared by D-043 P1 never reached PPO.
    return reward_.end_step();
}

StepResult Environment::step(const dp::agent::AgentAction& action) {
    const float dt = cfg_.fixed_dt;
    day_.advance(dt);

    // D-049 #2: blueprint line 62 "浣撳姏浣庡氨璧版參". Pre D-049 stamina was a
    // read-only number on the HUD - it decayed, but the agent's actual
    // move_speed_ never changed, so there was no reason for PPO to rest.
    // Now low stamina scales the move command before it reaches the body
    // via stamina_speed_multiplier (see Environment.hpp for the curve).
    dp::agent::AgentAction scaled = action;
    {
        float speed_mul = Environment::stamina_speed_multiplier(
            vital_state_.stamina);
        scaled.move_x *= speed_mul;
        scaled.move_y *= speed_mul;
    }

    // Agent body update first (so vitals etc see the new position).
    agent_.apply_action(scaled, world_, physics_, dt);

    const float step_reward_clipped = apply_action_effects_(scaled);

    // Cognitive map: write whatever the agent currently senses around it
    // into the spatial grid cells *within VISION_RANGE*, not just under
    // its feet. This is what gives PPO long-term spatial recall
    // ("I saw water in the NW corner earlier").
    //
    // D-049 #3: before this change the loop below wrote a single cell per
    // tick (the one under the agent's feet) and tagged it with whatever
    // was visible in a 60 m circle. That made cognitive map essentially
    // useless for navigation: the agent had to physically step ON each
    // water cell to remember "this is water", which never happens in a
    // 1024 m world. Blueprint line 24 says "鐭ラ亾瀹躲€佹按婧愩€佺嫾鍑烘病鍖哄湪鍝?,
    // i.e. *seen* things should be remembered. We now mark every grid
    // cell whose centre lies within VISION_RANGE of the agent with the
    // observed feature, matching the "vision = memory write" semantics.
    {
        const auto& as = agent_.state();
        const float R  = dp::agent::ObservationBuilder::VISION_RANGE;
        const float R2 = R * R;
        const float cell = dp::agent::SpatialMemory::CELL_SIZE_M;

        // Visited cell under the feet (kept for compatibility with the
        // visited channel; also writes danger/etc. for the immediate
        // spot).
        spatial_.write(as.pos_x, as.pos_y,
                       /*water*/false, /*food*/false,
                       /*resource*/false, /*danger*/false,
                       /*building*/false);

        // Stamp every water cell within vision range.
        for (float dy = -R; dy <= R; dy += cell) {
            for (float dx = -R; dx <= R; dx += cell) {
                if (dx*dx + dy*dy > R2) continue;
                float wx = as.pos_x + dx, wy = as.pos_y + dy;
                auto b = world_.biome_at(wx, wy);
                if (b == dp::world::Biome::River
                 || b == dp::world::Biome::Swamp) {
                    spatial_.write(wx, wy, /*water*/true, false, false,
                                   false, false);
                }
            }
        }

        // Plants / resources: stamp the cell that actually contains the
        // thing, as long as the agent can see it.
        for (const auto& pl : plants_.plants()) {
            if (!pl.ripe) continue;
            float ddx = pl.x - as.pos_x, ddy = pl.y - as.pos_y;
            if (ddx*ddx + ddy*ddy <= R2) {
                spatial_.write(pl.x, pl.y, false, /*food*/true, false,
                               false, false);
            }
        }
        for (const auto& rs : resources_.resources()) {
            if (!rs.available) continue;
            float ddx = rs.x - as.pos_x, ddy = rs.y - as.pos_y;
            if (ddx*ddx + ddy*ddy <= R2) {
                spatial_.write(rs.x, rs.y, false, false,
                               /*resource*/true, false, false);
            }
        }
        // Wolves: where they currently are *plus* under the agent's own
        // feet, so the danger map keeps track of "this region is risky".
        bool saw_wolf_close = false;
        for (const auto& an : animals_.animals()) {
            if (an.state == dp::world::AnimalState::Dead) continue;
            if (an.kind != dp::world::AnimalKind::Wolf) continue;
            float ddx = an.pos_x - as.pos_x, ddy = an.pos_y - as.pos_y;
            if (ddx*ddx + ddy*ddy <= R2) {
                spatial_.write(an.pos_x, an.pos_y, false, false, false,
                               /*danger*/true, false);
                saw_wolf_close = true;
            }
        }
        if (saw_wolf_close) {
            spatial_.write(as.pos_x, as.pos_y, false, false, false,
                           /*danger*/true, false);
        }
        for (const auto& bd : buildings_.buildings()) {
            float ddx = bd.x - as.pos_x, ddy = bd.y - as.pos_y;
            if (ddx*ddx + ddy*ddy <= R2) {
                spatial_.write(bd.x, bd.y, false, false, false, false,
                               /*building*/true);
            }
        }
        for (const auto& site : buildings_.sites()) {
            if (site.completed) continue;
            float ddx = site.x - as.pos_x, ddy = site.y - as.pos_y;
            if (ddx*ddx + ddy*ddy <= R2) {
                spatial_.write(site.x, site.y, false, false, false,
                               false, /*building*/true);
            }
        }
    }

    episodes_.tick(dt);

    bool done = episodes_.is_done(vital_state_);

    StepResult out;
    // D-044: feed PPO the CLIPPED reward (Weights::reward_clip_min/max).
    // The unclipped breakdown stays available via reward_.last_step() and
    // reward_.episode() for diagnostics / EpisodeStats logging.
    out.reward       = step_reward_clipped;
    out.done         = done;
    out.episode_id   = episodes_.episode_id();
    out.episode_step = episodes_.step_count();

    if (done) {
        last_ep_stats_ = reward_.episode();
        ++episodes_completed_;
    }

    rebuild_observation_(out.obs);
    ++step_index_;
    return out;
}

void Environment::rebuild_observation_(
    dp::agent::ObservationBuilder::Vector& out) const
{
    dp::agent::ObservationBuilder::compose(
        world_, agent_, vital_state_, day_,
        plants_, animals_, resources_,
        inventory_, shelter_, episodes_, progress_,
        buildings_, wardrobe_, farming_, tame_,
        selected_building_, spatial_, out);
}

// D-049 #2: stamina-aware speed multiplier. See Environment.hpp.
float Environment::stamina_speed_multiplier(float stamina) {
    float clamped = stamina < 0.0f ? 0.0f
                  : (stamina > 50.0f ? 50.0f : stamina);
    return 0.5f + clamped * 0.01f;   // 50 -> 1.0, 0 -> 0.5
}

// D-043: snapshot the 10 first_X flags as a Crafter-style milestone vector.
EpisodeMilestones Environment::milestones() const {
    EpisodeMilestones m;
    m.drink    = first_drink_fired_;
    m.eat      = first_eat_fired_;
    m.collect  = first_collect_fired_;
    m.spear    = first_spear_fired_;
    m.shelter  = first_shelter_fired_;
    m.clothing = first_clothing_fired_;
    m.seed     = first_seed_planted_fired_;
    m.house    = first_house_built_fired_;
    m.crop     = first_crop_harvested_fired_;
    m.follower = first_follower_fired_;
    return m;
}

}  // namespace dp::env
