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
      agent_(cfg.world.seed ^ cfg.instance_seed_xor)
{
    // Match main.cpp's deterministic init order so we get the exact same
    // world layout for given seed.
    decorations_.generate(cfg_.world.seed ^ 0xDEC0ULL, world_);
    plants_.initialize(cfg_.world.seed     ^ 0xF00DULL, world_, decorations_);
    animals_.initialize(cfg_.world.seed    ^ 0xA11AULL, world_);
    resources_.initialize(cfg_.world.seed  ^ 0xC011ULL, world_);

    float wx_max = world_.heightmap().world_size_x();
    float wy_max = world_.heightmap().world_size_y();
    spawn_x_ = wx_max * 0.5f;
    spawn_y_ = wy_max * 0.5f;
    spawn_z_ = world_.surface_height(spawn_x_, spawn_y_) + 5.0f;

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
    // milestone flags persist across resets on purpose (curriculum
    // signals are once-per-training-run, matching main.cpp behaviour).

    rebuild_observation_(obs_out);
}

void Environment::apply_action_effects_(const dp::agent::AgentAction& action) {
    const auto& as = agent_.state();
    const float dt = cfg_.fixed_dt;

    reward_.begin_step();
    reward_.event_alive(dt);

    // Progression: every metre travelled while alive counts.
    float step_speed = std::sqrt(as.vel_x * as.vel_x + as.vel_y * as.vel_y);
    progress_.on_explore_step(step_speed * dt);

    // Animals + shelter dampening (mirrors main.cpp).
    float night_factor = std::max(0.0f, -day_.sun_height_normalized());
    float perc_scale = 1.0f, spd_scale = 1.0f;
    bool  sheltered = shelter_.covers(as.pos_x, as.pos_y)
                   || buildings_.covers(as.pos_x, as.pos_y);
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
                reward_.event_eat(-15.0f);
            } else {
                if (kcal > 0.0f) {
                    reward_.event_eat(vitals_.eat(vital_state_, kcal));
                }
                if (ml   > 0.0f) {
                    reward_.event_drink(vitals_.drink(vital_state_, ml));
                }
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
    if (action.drink) {
        auto b = world_.biome_at(as.pos_x, as.pos_y);
        if (b == dp::world::Biome::River || b == dp::world::Biome::Swamp) {
            reward_.event_drink(vitals_.drink(vital_state_, 35.0f));
            auto fish = animals_.agent_attack(as.pos_x, as.pos_y,
                                              agent_.yaw(), 2.0f, 100.0f);
            if (fish.killed
                && animals_.animals()[fish.index].kind
                    == dp::world::AnimalKind::Fish) {
                reward_.event_eat(vitals_.eat(vital_state_, 20.0f));
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
                    reward_.event_collected_resource();
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
                }
            }
        }
    }

    // P -> place_blueprint
    if (action.place_blueprint) {
        buildings_.place_site(selected_building_,
                              as.pos_x, as.pos_y, as.pos_z, progress_);
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
        if (inventory_.craft_spear()) reward_.event_crafted_tool();
    }
    if (action.place_shelter) {
        if (shelter_.try_place(inventory_, as.pos_x, as.pos_y, as.pos_z)) {
            reward_.event_built_shelter();
        }
    }
    if (action.attack) {
        bool has_spear = inventory_.num_of(dp::agent::ItemKind::Spear) > 0;
        float reach  = has_spear ? 2.5f : 1.5f;
        float damage = has_spear ? 50.0f : 20.0f;
        auto hit = animals_.agent_attack(as.pos_x, as.pos_y,
                                         agent_.yaw(), reach, damage);
        if (hit.killed) {
            auto kind = animals_.animals()[hit.index].kind;
            if (kind == dp::world::AnimalKind::Wolf) {
                reward_.event_wolf_killed();
                reward_.event_eat(vitals_.eat(vital_state_, 40.0f));
                inventory_.add(dp::agent::ItemKind::Fur);
            } else if (kind == dp::world::AnimalKind::Rabbit) {
                reward_.event_eat(vitals_.eat(vital_state_, 15.0f));
                inventory_.add(dp::agent::ItemKind::Fur);
            } else if (kind == dp::world::AnimalKind::Deer) {
                reward_.event_eat(vitals_.eat(vital_state_, 40.0f));
                inventory_.add(dp::agent::ItemKind::Fur);
            } else if (kind == dp::world::AnimalKind::Fish) {
                reward_.event_eat(vitals_.eat(vital_state_, 20.0f));
            }
        }
    }

    if (action.craft_grass_dress) {
        if (inventory_.craft_grass_dress()) reward_.event_crafted_tool();
    }
    if (action.craft_fur_cloak) {
        if (inventory_.craft_fur_cloak()) reward_.event_crafted_tool();
    }
    if (action.wear_clothes) {
        if (inventory_.num_of(dp::agent::ItemKind::FurCloak) > 0) {
            wardrobe_.wear(inventory_, dp::agent::ItemKind::FurCloak);
        } else if (inventory_.num_of(dp::agent::ItemKind::GrassDress) > 0) {
            wardrobe_.wear(inventory_, dp::agent::ItemKind::GrassDress);
        }
    }

    if (action.plant_seed && progress_.can_farm()) {
        if (inventory_.num_of(dp::agent::ItemKind::Seed) > 0) {
            inventory_.remove_one(dp::agent::ItemKind::Seed);
            farming_.plant_seed(as.pos_x, as.pos_y, as.pos_z, last_seed_kind_);
            reward_.event_collected_resource();
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

    reward_.end_step();
}

StepResult Environment::step(const dp::agent::AgentAction& action) {
    const float dt = cfg_.fixed_dt;
    day_.advance(dt);

    // Agent body update first (so vitals etc see the new position).
    agent_.apply_action(action, world_, physics_, dt);

    apply_action_effects_(action);

    // Cognitive map: write whatever the agent currently senses around it
    // into the spatial grid cell under its feet. This is what gives PPO
    // long-term spatial recall ("I saw water in the NW corner earlier").
    {
        const auto& as = agent_.state();
        auto b = world_.biome_at(as.pos_x, as.pos_y);
        bool saw_water = (b == dp::world::Biome::River
                          || b == dp::world::Biome::Swamp);
        const float R = dp::agent::ObservationBuilder::VISION_RANGE;
        const float R2 = R * R;
        bool saw_food = false;
        for (const auto& pl : plants_.plants()) {
            if (!pl.ripe) continue;
            float dx = pl.x - as.pos_x, dy = pl.y - as.pos_y;
            if (dx*dx + dy*dy <= R2) { saw_food = true; break; }
        }
        bool saw_resource = false;
        for (const auto& rs : resources_.resources()) {
            if (!rs.available) continue;
            float dx = rs.x - as.pos_x, dy = rs.y - as.pos_y;
            if (dx*dx + dy*dy <= R2) { saw_resource = true; break; }
        }
        bool saw_danger = false;
        for (const auto& an : animals_.animals()) {
            if (an.state == dp::world::AnimalState::Dead) continue;
            if (an.kind != dp::world::AnimalKind::Wolf) continue;
            float dx = an.pos_x - as.pos_x, dy = an.pos_y - as.pos_y;
            if (dx*dx + dy*dy <= R2) { saw_danger = true; break; }
        }
        bool saw_building = false;
        for (const auto& bd : buildings_.buildings()) {
            float dx = bd.x - as.pos_x, dy = bd.y - as.pos_y;
            if (dx*dx + dy*dy <= R2) { saw_building = true; break; }
        }
        for (const auto& site : buildings_.sites()) {
            if (site.completed) continue;
            float dx = site.x - as.pos_x, dy = site.y - as.pos_y;
            if (dx*dx + dy*dy <= R2) { saw_building = true; break; }
        }
        spatial_.write(as.pos_x, as.pos_y,
                       saw_water, saw_food, saw_resource,
                       saw_danger, saw_building);
    }

    episodes_.tick(dt);

    bool done = episodes_.is_done(vital_state_);

    StepResult out;
    out.reward       = reward_.last_step().total();
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

}  // namespace dp::env
