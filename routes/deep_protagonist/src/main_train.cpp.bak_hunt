// deep_protagonist · headless trainer entry point.
//
// This is the binary the PPO loop will eventually drive. For S4-pre it
// just runs the Environment with a random/wander policy so we can:
//   * verify that Environment matches the viewer logic,
//   * benchmark headless tick rate,
//   * produce a baseline log to compare PPO against.
//
// CLI:
//   deep_protagonist_train --steps 10000          (default 10000)
//                          --episodes 100         (cap on episodes)
//                          --seed 12345           (world seed; default 12345)
//                          --policy wander|noop   (default wander)
//                          --metrics path.jsonl   (optional, append-mode)

#include "env/Environment.hpp"
#include "env/VecEnv.hpp"
#include "agent/AgentAction.hpp"
#include "policy/PPO.hpp"
#include "policy/BCTrainer.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <toml++/toml.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {

// D-101: map a discrete AgentAction back to the categorical index PPO uses
// (see PPO.cpp sample_action). At most one trigger should be set by the
// ScriptedSettler; if several are, the first in canonical order wins. No
// trigger -> NOOP (16).
inline int action_to_disc_idx(const dp::agent::AgentAction& a) {
    if (a.eat)                  return 0;
    if (a.drink)                return 1;
    if (a.attack)               return 2;
    if (a.collect)              return 3;
    if (a.place_shelter)        return 4;
    if (a.craft_spear)          return 5;
    if (a.sleep)                return 6;
    if (a.craft_grass_dress)    return 7;
    if (a.craft_fur_cloak)      return 8;
    if (a.wear_clothes)         return 9;
    if (a.place_blueprint)      return 10;
    if (a.cycle_building_type)  return 11;
    if (a.deposit_to_site)      return 12;
    if (a.plant_seed)           return 13;
    if (a.water_plot)           return 14;
    if (a.feed_tame)            return 15;
    if (a.tend_fire)            return dp::policy::ActorCriticImpl::TEND_FIRE_IDX;  // 17 (D-112 fire)
    return dp::policy::ActorCriticImpl::NOOP_IDX;  // 16
}

// D-101: invert the tanh squashing PPO applies (move = tanh(cont),
// yaw_rate = tanh(cont)*3) so the recorded continuous target is in the
// network's PRE-tanh output space. Clamp away from +/-1 to keep atanh finite.
inline void action_to_cont(const dp::agent::AgentAction& a, float out[3]) {
    auto inv = [](float v) {
        v = std::clamp(v, -0.999f, 0.999f);
        return 0.5f * std::log((1.0f + v) / (1.0f - v));   // atanh
    };
    out[0] = inv(a.move_x);
    out[1] = inv(a.move_y);
    out[2] = inv(a.yaw_rate / 3.0f);
}

std::vector<std::string> split_csv(const std::string& s) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        if (!tok.empty()) out.push_back(tok);
    }
    return out;
}

dp::world::World::Config load_world_config(const std::string& path) {
    dp::world::World::Config cfg;
    if (!std::filesystem::exists(path)) {
        spdlog::warn("Config file '{}' missing; using defaults.", path);
        return cfg;
    }
    auto t = toml::parse_file(path);
    cfg.heightmap_cells_x   = (int)   t["world"]["heightmap_cells_x"].value_or<int64_t>(cfg.heightmap_cells_x);
    cfg.heightmap_cells_y   = (int)   t["world"]["heightmap_cells_y"].value_or<int64_t>(cfg.heightmap_cells_y);
    cfg.cell_scale          = (float) t["world"]["cell_scale"].value_or<double>(cfg.cell_scale);
    cfg.terrain_max_height  = (float) t["world"]["max_height"].value_or<double>(cfg.terrain_max_height);
    cfg.terrain_frequency   = (float) t["world"]["terrain_frequency"].value_or<double>(cfg.terrain_frequency);
    cfg.terrain_octaves     = (int)   t["world"]["terrain_octaves"].value_or<int64_t>(cfg.terrain_octaves);
    cfg.terrain_persistence = (float) t["world"]["terrain_persistence"].value_or<double>(cfg.terrain_persistence);
    cfg.cave_count          = (int)   t["world"]["cave_count"].value_or<int64_t>(cfg.cave_count);
    cfg.river_count         = (int)   t["world"]["river_count"].value_or<int64_t>(cfg.river_count);
    cfg.seed                = (uint64_t) t["world"]["seed"].value_or<int64_t>(12345);
    return cfg;
}

// Wander policy: random 2D heading every ~2s (mirrors CapsuleAgent's
// wander), 5% chance per step to fire each interaction trigger. Gives a
// noisy baseline policy that DOES touch every action channel - useful
// for shaking out env bugs.
// Per-episode action histogram for C-diagnostic. Counts how many ticks
// each of the 16 discrete triggers was set, plus mean |move| and |yaw|.
// Index map (matches AgentAction discrete order):
//   0:eat 1:drink 2:attack 3:collect 4:place_shelter 5:craft_spear 6:sleep
//   7:craft_grass_dress 8:craft_fur_cloak 9:wear_clothes
//   10:place_blueprint 11:cycle_building_type 12:deposit_to_site
//   13:plant_seed 14:water_plot 15:feed_tame
//   16:NOOP (not tracked here)  17:tend_fire (D-112 fire token)
struct ActionStats {
    static constexpr int N = 18;   // 16 triggers + NOOP slot (16) + tend_fire (17)
    int disc_count[N] = {0};
    double move_mag_sum = 0.0;
    double yaw_abs_sum  = 0.0;
    int    ticks        = 0;

    void accumulate(const dp::agent::AgentAction& a) {
        disc_count[ 0] += a.eat                 ? 1 : 0;
        disc_count[ 1] += a.drink               ? 1 : 0;
        disc_count[ 2] += a.attack              ? 1 : 0;
        disc_count[ 3] += a.collect             ? 1 : 0;
        disc_count[ 4] += a.place_shelter       ? 1 : 0;
        disc_count[ 5] += a.craft_spear         ? 1 : 0;
        disc_count[ 6] += a.sleep               ? 1 : 0;
        disc_count[ 7] += a.craft_grass_dress   ? 1 : 0;
        disc_count[ 8] += a.craft_fur_cloak     ? 1 : 0;
        disc_count[ 9] += a.wear_clothes        ? 1 : 0;
        disc_count[10] += a.place_blueprint     ? 1 : 0;
        disc_count[11] += a.cycle_building_type ? 1 : 0;
        disc_count[12] += a.deposit_to_site     ? 1 : 0;
        disc_count[13] += a.plant_seed          ? 1 : 0;
        disc_count[14] += a.water_plot          ? 1 : 0;
        disc_count[15] += a.feed_tame           ? 1 : 0;
        disc_count[17] += a.tend_fire           ? 1 : 0;  // D-112 fire token
        move_mag_sum += std::sqrt(a.move_x * a.move_x + a.move_y * a.move_y);
        yaw_abs_sum  += std::fabs(a.yaw_rate);
        ++ticks;
    }

    void reset() {
        for (int i = 0; i < N; ++i) disc_count[i] = 0;
        move_mag_sum = 0.0;
        yaw_abs_sum  = 0.0;
        ticks        = 0;
    }
};

struct WanderPolicy {
    std::mt19937_64 rng;
    float dirx = 1.0f, diry = 0.0f;
    float change_in = 0.0f;

    explicit WanderPolicy(uint64_t seed) : rng(seed ? seed : 1) {}

    float frand01() {
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
    }

    bool roll(float p) { return frand01() < p; }

    dp::agent::AgentAction sample(float dt) {
        change_in -= dt;
        if (change_in <= 0.0f) {
            float ang = frand01() * 6.2831853f;
            dirx = std::cos(ang);
            diry = std::sin(ang);
            change_in = 2.0f;
        }
        dp::agent::AgentAction a;
        a.move_x = dirx;
        a.move_y = diry;
        // Each interaction has a small chance per step to fire.
        a.eat                 = roll(0.02f);
        a.drink               = roll(0.02f);
        a.attack              = roll(0.01f);
        a.collect             = roll(0.05f);
        a.deposit_to_site     = roll(0.02f);
        a.place_shelter       = roll(0.001f);
        a.craft_spear         = roll(0.001f);
        a.craft_grass_dress   = roll(0.001f);
        a.craft_fur_cloak     = roll(0.001f);
        a.wear_clothes        = roll(0.005f);
        a.place_blueprint     = roll(0.0005f);
        a.cycle_building_type = roll(0.005f);
        a.plant_seed          = roll(0.005f);
        a.water_plot          = roll(0.01f);
        a.feed_tame           = roll(0.005f);
        return a;
    }
};

// D-087 (user mandate "改架构改算法"): rule-based scripted policy that
// demonstrates the blueprint S1->S3 chain. Reads the observation vector
// (layout in include/agent/Observation.hpp) and produces actions that
// (a) walk to the auto-placed Shed site (D-086 admin_place_site at
//     spawn_y+8), (b) deposit one Wood + one Grass to complete it,
// (c) keep drinking/eating to survive long enough.
//
// Purpose 1 (verification): does the env actually permit the blueprint
//   scene? 5 PPO runs * 5M total built ~7 houses; this lets us check
//   "scripted = 1 house per ep" before trusting BC/demos.
// Purpose 2 (demo source): once verified, we can record (obs, action)
//   pairs from this policy and BC-pretrain a PPO net (D-088).
struct ScriptedPolicy {
    std::mt19937_64 rng;
    float dirx = 0.0f, diry = 1.0f;   // initial: face north toward site
    int   tick = 0;
    int   deposit_cooldown = 0;

    static constexpr int OBS_HUNGER       = 0;
    static constexpr int OBS_THIRST       = 1;
    static constexpr int OBS_INV_WOOD     = 117;
    static constexpr int OBS_INV_STONE    = 118;
    static constexpr int OBS_INV_GRASS    = 119;
    static constexpr int OBS_STAGE_MID    = 125;
    static constexpr int OBS_BLDG_DX      = 130;
    static constexpr int OBS_BLDG_DY      = 131;
    static constexpr int OBS_BLDG_DONE    = 132;
    static constexpr int OBS_WATER_DX     = 145;
    static constexpr int OBS_WATER_DY     = 146;
    static constexpr int OBS_WATER_DIST   = 147;
    static constexpr int OBS_RES_BASE     = 99;
    static constexpr int OBS_RES_STRIDE   = 6;
    static constexpr int OBS_PLANT_BASE   = 39;
    static constexpr int OBS_PLANT_STRIDE = 7;
    static constexpr float VISION_RANGE   = 60.0f;

    explicit ScriptedPolicy(uint64_t seed) : rng(seed ? seed : 1) {}

    bool find_resource_(const dp::agent::ObservationBuilder::Vector& obs,
                        int kind, float& out_dx, float& out_dy) const {
        float best_d2 = 1e9f;
        bool  found   = false;
        for (int i = 0; i < 3; ++i) {
            int b = OBS_RES_BASE + i * OBS_RES_STRIDE;
            float dx = obs[b + 0];
            float dy = obs[b + 1];
            float onehot = obs[b + 3 + kind];
            if (onehot < 0.5f) continue;
            float d2 = dx * dx + dy * dy;
            if (d2 < best_d2) { best_d2 = d2; out_dx = dx; out_dy = dy; found = true; }
        }
        return found;
    }

    bool find_plant_(const dp::agent::ObservationBuilder::Vector& obs,
                     float& out_dx, float& out_dy) const {
        float best_d2 = 1e9f;
        bool  found   = false;
        for (int i = 0; i < 4; ++i) {
            int b = OBS_PLANT_BASE + i * OBS_PLANT_STRIDE;
            float dx = obs[b + 0];
            float dy = obs[b + 1];
            if (std::fabs(dx) < 1e-6f && std::fabs(dy) < 1e-6f) continue;
            float d2 = dx * dx + dy * dy;
            if (d2 < best_d2) { best_d2 = d2; out_dx = dx; out_dy = dy; found = true; }
        }
        return found;
    }

    void steer_(dp::agent::AgentAction& a, float tx, float ty) {
        float mag = std::sqrt(tx * tx + ty * ty);
        if (mag < 1e-4f) { a.move_x = dirx; a.move_y = diry; return; }
        a.move_x = tx / mag;
        a.move_y = ty / mag;
        dirx = a.move_x;
        diry = a.move_y;
    }

    dp::agent::AgentAction sample(const dp::agent::ObservationBuilder::Vector& obs,
                                  float /*dt*/) {
        ++tick;
        if (deposit_cooldown > 0) --deposit_cooldown;
        dp::agent::AgentAction a;

        float thirst = obs[OBS_THIRST];
        float hunger = obs[OBS_HUNGER];
        float inv_w  = obs[OBS_INV_WOOD];
        float inv_g  = obs[OBS_INV_GRASS];
        float site_dx   = obs[OBS_BLDG_DX];
        float site_dy   = obs[OBS_BLDG_DY];
        float site_done = obs[OBS_BLDG_DONE];
        float site_dist_norm = std::sqrt(site_dx * site_dx + site_dy * site_dy);
        bool  site_active    = (site_dist_norm > 1e-4f) && (site_done < 0.5f);

        const float THIRST_CRIT = 0.30f;
        const float HUNGER_CRIT = 0.30f;
        const float THIRST_LOW  = 0.55f;
        const float HUNGER_LOW  = 0.55f;
        const float DEPOSIT_RANGE_NORM = 2.0f / VISION_RANGE;
        const float WATER_DRINK_RANGE  = 2.0f / VISION_RANGE;
        const float PLANT_EAT_RANGE    = 1.5f / VISION_RANGE;
        const float COLLECT_RANGE      = 2.0f / VISION_RANGE;

        bool have_wood  = inv_w > 0.005f;
        bool have_grass = inv_g > 0.005f;
        bool can_deposit = site_active && (have_wood || have_grass);

        if (thirst < THIRST_CRIT) {
            float wx = obs[OBS_WATER_DX];
            float wy = obs[OBS_WATER_DY];
            float wd = obs[OBS_WATER_DIST];
            if (wd > 1e-4f) {
                steer_(a, wx, wy);
                if (wd < WATER_DRINK_RANGE) a.drink = true;
                return a;
            }
        }
        if (hunger < HUNGER_CRIT) {
            float px, py;
            if (find_plant_(obs, px, py)) {
                steer_(a, px, py);
                float pd = std::sqrt(px * px + py * py);
                if (pd < PLANT_EAT_RANGE) a.eat = true;
                return a;
            }
        }

        if (can_deposit) {
            if (site_dist_norm < DEPOSIT_RANGE_NORM) {
                a.move_x = 0.0f;
                a.move_y = 0.0f;
                if (deposit_cooldown == 0) {
                    a.deposit_to_site = true;
                    deposit_cooldown  = 3;
                }
                return a;
            }
            steer_(a, site_dx, site_dy);
            return a;
        }

        if (site_active && !can_deposit) {
            float rx, ry;
            int   need = have_wood ? 2 : 0;
            if (find_resource_(obs, need, rx, ry)) {
                steer_(a, rx, ry);
                float rd = std::sqrt(rx * rx + ry * ry);
                if (rd < COLLECT_RANGE) a.collect = true;
                return a;
            }
            int alt = have_wood ? 0 : 2;
            if (find_resource_(obs, alt, rx, ry)) {
                steer_(a, rx, ry);
                float rd = std::sqrt(rx * rx + ry * ry);
                if (rd < COLLECT_RANGE) a.collect = true;
                return a;
            }
        }

        if (thirst < THIRST_LOW) {
            float wx = obs[OBS_WATER_DX];
            float wy = obs[OBS_WATER_DY];
            float wd = obs[OBS_WATER_DIST];
            if (wd > 1e-4f) {
                steer_(a, wx, wy);
                if (wd < WATER_DRINK_RANGE) a.drink = true;
                return a;
            }
        }
        if (hunger < HUNGER_LOW) {
            float px, py;
            if (find_plant_(obs, px, py)) {
                steer_(a, px, py);
                float pd = std::sqrt(px * px + py * py);
                if (pd < PLANT_EAT_RANGE) a.eat = true;
                return a;
            }
        }

        if ((tick & 31) == 0) {
            float ang = std::uniform_real_distribution<float>(0.0f, 6.2831853f)(rng);
            dirx = std::cos(ang);
            diry = std::sin(ang);
        }
        a.move_x = dirx;
        a.move_y = diry;
        return a;
    }
};

// =============================================================================
// D-100: ScriptedSettler — 7-layer nested reactive policy. Designed so a PPO
// network trained via Behavioral Cloning on its (obs, action) pairs learns
// MECHANISMS (and their cross-dependencies), not a fixed trajectory.
//
//   Layer 0  context — derive `is_night`, `at_home`, `wolf_close`, etc. ONCE
//                      from obs, share across all subsequent layers.
//   Layer 1  EMERGENCY — vital about to hit 0 AND a save action is in reach.
//                        Overrides everything else. Includes "night + about to
//                        freeze far from home -> sprint home" rule.
//   Layer 2  SURVIVAL — wolf nearby or night-far-from-home. Drop secondary
//                        goals. Either fight (have spear + healthy), shelter
//                        (run home / cave / Shed), or retreat.
//   Layer 3  VITAL MAINT — thirst/hunger trending low but not critical.
//                          Decision branches on day/night AND distance to
//                          target. Night + far source = stay (lose vital
//                          slowly is better than freezing).
//   Layer 4  CONSTRUCTION — site exists & not done. Sub-branches: have all
//                            mats -> deposit; missing wood -> tree; missing
//                            grass -> grass; missing stone -> stone. Pick
//                            nearest needed.
//   Layer 5  SETTLEMENT — home complete + vitals ok. If far from home, walk
//                          home. If at home, idle (move=0). NOT random wander.
//                          This is the layer that creates `bldg_ticks`.
//   Layer 6  CRAFTING — at home + Mid stage + materials in inv. Make spear,
//                        clothes, wear them. Triggers are cooldowned so we
//                        don't spam.
//   Layer 7  EXPLORATION — only when ALL above are satisfied. Short scouting
//                           walks (8m radius) to gather wood/grass for spares.
//
// All distances are normalised by VISION_RANGE = 60m. We never use absolute
// coordinates so each tick decision is purely obs -> action. Replayed with a
// different seed the bot walks a different path but applies the same logic.
// =============================================================================
struct ScriptedSettler {
    std::mt19937_64 rng;
    float dirx = 0.0f, diry = 1.0f;
    int   tick = 0;
    int   deposit_cooldown = 0;
    int   craft_cooldown   = 0;
    int   wear_cooldown    = 0;
    // D-112 fire: the oracle has no fire channel in obs, so it tracks its own
    // estimate of remaining campfire fuel (in ticks) to decide when to
    // (re)light. Approximates FireSystem fuel_per_wood (90s) at the 30Hz
    // headless dt. fire_cooldown debounces the tend_fire trigger.
    static constexpr int FIRE_FUEL_TICKS = 2700;   // ~90s at 30Hz
    int   fire_timer       = 0;
    int   fire_cooldown    = 0;
    bool  no_fire          = false;   // D-114: gate fire (tend_fire) branches off for fire-free DAgger relabel

    // ---- obs indices (match include/agent/Observation.hpp layout) ----
    static constexpr int OBS_HUNGER       = 0;
    static constexpr int OBS_THIRST       = 1;
    static constexpr int OBS_STAMINA      = 2;
    static constexpr int OBS_TEMP         = 3;
    static constexpr int OBS_INSIDE_CAVE  = 9;
    static constexpr int OBS_DAY_SIN      = 12;
    static constexpr int OBS_DAY_COS      = 13;
    static constexpr int OBS_PLANT_BASE   = 39;
    static constexpr int OBS_PLANT_STRIDE = 7;
    static constexpr int OBS_RES_BASE     = 99;
    static constexpr int OBS_RES_STRIDE   = 6;
    static constexpr int OBS_INV_WOOD     = 117;
    static constexpr int OBS_INV_STONE    = 118;
    static constexpr int OBS_INV_GRASS    = 119;
    static constexpr int OBS_INV_SPEAR    = 120;
    static constexpr int OBS_STAGE_EARLY  = 124;
    static constexpr int OBS_STAGE_MID    = 125;
    static constexpr int OBS_STAGE_LATE   = 126;
    static constexpr int OBS_BLDG_DX      = 130;
    static constexpr int OBS_BLDG_DY      = 131;
    static constexpr int OBS_BLDG_DONE    = 132;
    static constexpr int OBS_INV_FUR      = 133;
    static constexpr int OBS_INV_DRESS    = 134;
    static constexpr int OBS_INV_CLOAK    = 135;
    static constexpr int OBS_WORN_NONE    = 137;
    static constexpr int OBS_WATER_DX     = 145;
    static constexpr int OBS_WATER_DY     = 146;
    static constexpr int OBS_WATER_DIST   = 147;
    static constexpr int OBS_WOLF_DX      = 148;
    static constexpr int OBS_WOLF_DY      = 149;
    static constexpr int OBS_WOLF_DIST    = 150;
    static constexpr int OBS_SITE_W_LEFT  = 161;
    static constexpr int OBS_SITE_S_LEFT  = 162;
    static constexpr int OBS_SITE_G_LEFT  = 163;
    // 4 nearest animals: base 67 (= plants 39 + 4*7), stride 8
    // (dx,dy,dz + 5 kind one-hots: Rabbit0 Deer1 Wolf2 Fish3 unused4).
    static constexpr int OBS_ANIM_BASE    = 67;
    static constexpr int OBS_ANIM_STRIDE  = 8;
    // decision_blueprint_nutrition §3a: dual nutrition bars appended at the
    // tail (normalised /100). OFF -> read 1.0 so nutrition branches stay dark.
    static constexpr int OBS_PROTEIN      = 567;
    static constexpr int OBS_VITAMIN      = 568;
    static constexpr float VISION_RANGE   = 60.0f;

    explicit ScriptedSettler(uint64_t seed) : rng(seed ? seed : 1) {}

    // Find nearest resource of a given kind (0=wood, 1=stone, 2=grass) by
    // scanning the 3 nearest-resource slots. Returns false if none of that
    // kind is in vision.
    bool find_resource_(const dp::agent::ObservationBuilder::Vector& obs,
                        int kind, float& out_dx, float& out_dy) const {
        float best = 1e9f;
        bool  found = false;
        for (int i = 0; i < 3; ++i) {
            int b = OBS_RES_BASE + i * OBS_RES_STRIDE;
            float onehot = obs[b + 3 + kind];
            if (onehot < 0.5f) continue;
            float dx = obs[b + 0], dy = obs[b + 1];
            float d2 = dx*dx + dy*dy;
            if (d2 < best) { best = d2; out_dx = dx; out_dy = dy; found = true; }
        }
        return found;
    }

    // Nearest ripe plant in the 4 plant slots.
    bool find_plant_(const dp::agent::ObservationBuilder::Vector& obs,
                     float& out_dx, float& out_dy) const {
        float best = 1e9f;
        bool  found = false;
        for (int i = 0; i < 4; ++i) {
            int b = OBS_PLANT_BASE + i * OBS_PLANT_STRIDE;
            float dx = obs[b + 0], dy = obs[b + 1];
            if (std::fabs(dx) < 1e-6f && std::fabs(dy) < 1e-6f) continue;
            float d2 = dx*dx + dy*dy;
            if (d2 < best) { best = d2; out_dx = dx; out_dy = dy; found = true; }
        }
        return found;
    }

    // Nearest huntable prey (Rabbit=0 / Deer=1 / Fish=3; Wolf=2 excluded — a
    // wolf is a predator, not a meal the oracle should chase) across the 4
    // animal slots. Used by the nutrition layer to refill the PROTEIN bar.
    bool find_prey_(const dp::agent::ObservationBuilder::Vector& obs,
                    float& out_dx, float& out_dy) const {
        float best = 1e9f;
        bool  found = false;
        for (int i = 0; i < 4; ++i) {
            int b = OBS_ANIM_BASE + i * OBS_ANIM_STRIDE;
            bool is_prey = obs[b + 3] > 0.5f      // Rabbit
                        || obs[b + 4] > 0.5f      // Deer
                        || obs[b + 6] > 0.5f;     // Fish
            if (!is_prey) continue;
            float dx = obs[b + 0], dy = obs[b + 1];
            if (std::fabs(dx) < 1e-6f && std::fabs(dy) < 1e-6f) continue;
            float d2 = dx*dx + dy*dy;
            if (d2 < best) { best = d2; out_dx = dx; out_dy = dy; found = true; }
        }
        return found;
    }

    void steer_(dp::agent::AgentAction& a, float tx, float ty) {
        float mag = std::sqrt(tx*tx + ty*ty);
        if (mag < 1e-4f) { a.move_x = dirx; a.move_y = diry; return; }
        a.move_x = tx / mag;
        a.move_y = ty / mag;
        dirx = a.move_x;
        diry = a.move_y;
    }

    // Walk toward an obs-space target (already normalised) and trigger
    // `trigger_when_in` action if within range_norm. Used by Layers 3-5.
    void walk_or_trigger_(dp::agent::AgentAction& a,
                          float tx, float ty,
                          float range_norm,
                          bool* trigger_flag,
                          int*  cooldown,
                          int   cd_ticks) {
        steer_(a, tx, ty);
        float d = std::sqrt(tx*tx + ty*ty);
        if (d < range_norm && trigger_flag && cooldown) {
            if (*cooldown == 0) {
                *trigger_flag = true;
                *cooldown = cd_ticks;
            }
        }
    }

    dp::agent::AgentAction sample(const dp::agent::ObservationBuilder::Vector& obs,
                                  float /*dt*/) {
        ++tick;
        if (deposit_cooldown > 0) --deposit_cooldown;
        if (craft_cooldown   > 0) --craft_cooldown;
        if (wear_cooldown    > 0) --wear_cooldown;
        if (fire_cooldown    > 0) --fire_cooldown;
        if (fire_timer       > 0) --fire_timer;   // D-112: burn down fuel estimate
        dp::agent::AgentAction a;

        // ---------- Layer 0: context ----------
        const float hunger  = obs[OBS_HUNGER];
        const float thirst  = obs[OBS_THIRST];
        const float temp_v  = obs[OBS_TEMP];
        const float day_cos = obs[OBS_DAY_COS];
        const float sun_h   = -day_cos;                  // matches Daylight::sun_height_normalized()
        const bool  is_night    = sun_h < -0.1f;
        const bool  deep_night  = sun_h < -0.4f;
        const bool  approaching_night = (!is_night) && sun_h < 0.15f;

        const float inv_w = obs[OBS_INV_WOOD];
        const float inv_s = obs[OBS_INV_STONE];
        const float inv_g = obs[OBS_INV_GRASS];
        const float inv_fur = obs[OBS_INV_FUR];
        const float inv_dress = obs[OBS_INV_DRESS];
        const float inv_cloak = obs[OBS_INV_CLOAK];
        const bool  have_wood  = inv_w > 0.005f;
        const bool  have_stone = inv_s > 0.005f;
        const bool  have_grass = inv_g > 0.005f;
        const bool  have_spear = obs[OBS_INV_SPEAR] > 0.005f;
        const bool  worn_none  = obs[OBS_WORN_NONE]  > 0.5f;

        const float home_dx     = obs[OBS_BLDG_DX];
        const float home_dy     = obs[OBS_BLDG_DY];
        const float home_done   = obs[OBS_BLDG_DONE];
        const float home_d2     = home_dx*home_dx + home_dy*home_dy;
        const float home_d_norm = std::sqrt(home_d2);
        const float home_d_m    = home_d_norm * VISION_RANGE;
        const bool  home_complete = home_done > 0.5f;
        const bool  have_home_anchor = home_d_norm > 1e-4f;  // false only if no site ever placed
        const bool  at_home = have_home_anchor && home_d_m < 8.0f;
        const bool  near_home = have_home_anchor && home_d_m < 15.0f;
        const bool  inside_cave = obs[OBS_INSIDE_CAVE] > 0.5f;
        const bool  sheltered_now = at_home || inside_cave;

        const float wolf_dist_norm = obs[OBS_WOLF_DIST];
        const float wolf_dx        = obs[OBS_WOLF_DX];
        const float wolf_dy        = obs[OBS_WOLF_DY];
        const float wolf_d_m       = wolf_dist_norm * VISION_RANGE;
        const bool  wolf_visible   = wolf_dist_norm > 1e-4f;
        const bool  wolf_close     = wolf_visible && wolf_d_m < 12.0f;
        const bool  wolf_critical  = wolf_visible && wolf_d_m < 6.0f;

        const float water_dx    = obs[OBS_WATER_DX];
        const float water_dy    = obs[OBS_WATER_DY];
        const float water_dnorm = obs[OBS_WATER_DIST];
        const float water_d_m   = water_dnorm * VISION_RANGE;
        const bool  water_visible = water_dnorm > 1e-4f;

        const bool stage_mid  = obs[OBS_STAGE_MID]  > 0.5f;
        const bool stage_late = obs[OBS_STAGE_LATE] > 0.5f;
        const bool site_active = have_home_anchor && !home_complete;
        const bool site_needs_w = obs[OBS_SITE_W_LEFT] > 0.005f;
        const bool site_needs_s = obs[OBS_SITE_S_LEFT] > 0.005f;
        const bool site_needs_g = obs[OBS_SITE_G_LEFT] > 0.005f;

        // Thresholds reused below
        const float TH_CRIT_LOW  = 0.20f;   // thirst/hunger < 20% -> emergency
        const float TH_LOW       = 0.55f;   // moderate need
        const float TH_OK_HIGH   = 0.80f;   // restocked
        const float TEMP_CRIT    = 0.15f;   // body temp 15/100 -> freezing
        const float TEMP_LOW     = 0.30f;
        const float DRINK_RANGE  = 2.0f  / VISION_RANGE;
        const float EAT_RANGE    = 1.5f  / VISION_RANGE;
        const float COLLECT_RANGE= 2.0f  / VISION_RANGE;
        const float DEPOSIT_RANGE= 2.0f  / VISION_RANGE;
        const float WATER_FAR_M  = 25.0f;   // beyond this is "expensive"
        const float HOME_CORE_M  = 3.0f;    // D-102: tight "indoors" core. At
                                            // night the settler returns to and
                                            // rests at this core (sleep) rather
                                            // than foraging; the active return
                                            // gives BC a restoring-force example
                                            // at every drift distance.

        // ---------- Layer 1: EMERGENCY ----------
        // 1a) thirst about to hit 0 AND water in arm's reach (day or night)
        if (thirst < TH_CRIT_LOW && water_visible && water_d_m < 4.0f) {
            steer_(a, water_dx, water_dy);
            a.drink = true;
            return a;
        }
        // 1b) hunger about to hit 0 AND plant in arm's reach
        if (hunger < TH_CRIT_LOW) {
            float px, py;
            if (find_plant_(obs, px, py) && std::sqrt(px*px + py*py) < EAT_RANGE) {
                steer_(a, px, py);
                a.eat = true;
                return a;
            }
        }
        // 1c) FREEZING (D-112 fire rescue). A campfire is instant warmth THIS
        // tick -- VitalSystem applies the heat source before the death check --
        // whereas a sprint home keeps draining temperature for every metre en
        // route, which is exactly how the residual freeze deaths happen (agent
        // caught out at night, walks home, dies on the way). So when truly
        // cold: if we have wood and home is not already at our feet, build a
        // fire and hug it; survival beats construction, so this ignores the
        // build-protection gate. Only sprint home when it is essentially
        // adjacent. With no wood, fall back to the old best-effort sprint home.
        if (temp_v < TEMP_CRIT) {
            const bool home_adjacent = have_home_anchor && home_d_m < 8.0f;
            if (!no_fire && have_wood && !home_adjacent) {
                if (fire_cooldown == 0) {
                    a.tend_fire   = true;
                    fire_cooldown = 6;
                    fire_timer    = FIRE_FUEL_TICKS;
                    return a;
                }
                a.move_x = 0.0f; a.move_y = 0.0f;  // stand in radius, absorb warmth
                return a;
            }
            if (have_home_anchor && home_d_m < 40.0f) {
                steer_(a, home_dx, home_dy);
                return a;
            }
        }
        // 1d) very thirsty + night + water far -> stay/return home, hold the loss
        if (thirst < TH_CRIT_LOW && is_night && have_home_anchor && !at_home) {
            steer_(a, home_dx, home_dy);
            return a;
        }

        // ---------- Layer 2: SURVIVAL ----------
        // 2a) wolf close
        if (wolf_close) {
            // 2a.i fight if armed + healthy enough
            if (have_spear && hunger > TH_LOW && thirst > TH_LOW
                && wolf_critical) {
                steer_(a, wolf_dx, wolf_dy);
                a.attack = true;
                return a;
            }
            // 2a.ii at home -> stay put, wolves get 0.3x perception 0.7x speed
            if (at_home) {
                a.move_x = 0.0f; a.move_y = 0.0f;
                return a;
            }
            // 2a.iii have_home_anchor -> retreat toward home, away from wolf
            if (have_home_anchor) {
                float bx = home_dx, by = home_dy;
                steer_(a, bx, by);
                return a;
            }
            // 2a.iv last resort: flee directly away from wolf
            steer_(a, -wolf_dx, -wolf_dy);
            return a;
        }
        // 2a') NIGHT FIRE CAMP (D-112). If night is here or imminent and home
        // is too far to reach before freezing (or we have none), don't make a
        // doomed dash home -- make camp by a fire instead. This is the
        // dominant residual freeze-death mode: the agent strays far foraging
        // and gets caught out in the dark with home out of reach. With wood we
        // keep a fire lit and hug it through the night; without wood at dusk we
        // grab the nearest wood now so we can light one. Home, when reachable,
        // is still preferred and handled by 2b/2c below for the near-home case,
        // so this only fires when home is genuinely too far (or absent).
        {
            const bool night_or_dusk = is_night || approaching_night;
            const bool home_far = !have_home_anchor || home_d_m > 28.0f;
            if (!no_fire && night_or_dusk && home_far) {
                if (have_wood) {
                    if (fire_cooldown == 0 && fire_timer <= (FIRE_FUEL_TICKS / 2)) {
                        a.tend_fire   = true;
                        fire_cooldown = 6;
                        fire_timer    = FIRE_FUEL_TICKS;
                        return a;
                    }
                    if (is_night) {          // fire burning -> stay in its radius
                        a.move_x = 0.0f; a.move_y = 0.0f;
                        return a;
                    }
                } else if (!is_night
                           && hunger > TH_CRIT_LOW && thirst > TH_CRIT_LOW) {
                    // Dusk, no wood -> fetch the nearest wood to light a fire.
                    float wx, wy;
                    if (find_resource_(obs, 0, wx, wy)) {
                        steer_(a, wx, wy);
                        if (std::sqrt(wx*wx + wy*wy) < COLLECT_RANGE) a.collect = true;
                        return a;
                    }
                }
            }
        }

        // 2b) night + far from home + home complete (we built it, USE it)
        if (is_night && have_home_anchor && !near_home) {
            steer_(a, home_dx, home_dy);
            return a;
        }
        // 2c) approaching night AND far from home AND inv has enough -> walk home
        if (approaching_night && have_home_anchor && home_d_m > 15.0f
            && thirst > 0.40f && hunger > 0.40f) {
            steer_(a, home_dx, home_dy);
            return a;
        }

        // ---------- Layer 3N: NUTRITION MAINTENANCE ----------
        // decision_blueprint_world_coherence §4/§5a: protein (obs 567) and
        // vitamin (obs 568) decay independently of hunger/thirst. The caveman
        // must seek the RIGHT food for the RIGHT deficiency: forage ripe plants
        // -> vitamin, hunt prey (rabbit/deer/fish) -> protein. This is the demo
        // behaviour the new BC base learns; it also makes the §4 survivability
        // gate real (a forager/hunter lives, a neglecter starves). When
        // nutrition is OFF both bars read 1.0 so neither branch fires and the
        // old-world oracle is byte-for-byte unchanged. Day/night risk gated
        // exactly like 3a/3b so chasing food never causes a freeze death.
        {
            const float protein = obs[OBS_PROTEIN];
            const float vitamin = obs[OBS_VITAMIN];
            const float NUTRI_LOW = 0.45f;   // act before the 0.30 debuff bites
            const bool  have_spear_n = obs[OBS_INV_SPEAR] > 0.005f;
            const bool  have_wood_n  = obs[OBS_INV_WOOD]  > 0.005f;
            const bool  have_stone_n = obs[OBS_INV_STONE] > 0.005f;
            const float ATTACK_RANGE = (have_spear_n ? 2.3f : 1.4f) / VISION_RANGE;
            const bool  need_vit = vitamin < NUTRI_LOW;
            const bool  need_pro = protein < NUTRI_LOW;

            // Vitamin: forage the nearest ripe plant (action.eat).
            auto try_forage = [&]() -> bool {
                float px, py;
                if (!find_plant_(obs, px, py)) return false;
                float pd2  = px*px + py*py;
                float pd_m = std::sqrt(pd2) * VISION_RANGE;
                if (is_night && pd_m > 10.0f) return false;
                steer_(a, px, py);
                if (std::sqrt(pd2) < EAT_RANGE) a.eat = true;
                return true;
            };
            // Protein: meat. Three sources, ordered by reliability:
            //   1. Arm in-place: craft_spear is inventory-only (Environment.cpp)
            //      so we forge a spear the moment protein bites, not just at
            //      home (Layer 6 only crafts when at_home, which never fires if
            //      the auto-Shed sits on spawn). 50 dmg/2.5 m one-/two-shots.
            //   2. FISH at water: the drink action auto-attacks a fish within
            //      2 m and "fish is meat too" -> eat_protein. Water sits near
            //      the settlement and fish don't flee like rabbits/deer, so this
            //      is the DEPENDABLE protein source (historical land-hunt
            //      success ~8%). Drinking also tops thirst -- pure upside.
            //   3. Opportunistic land hunt if a prey is already close.
            auto try_protein = [&]() -> bool {
                if (!have_spear_n && have_wood_n && have_stone_n
                    && craft_cooldown == 0) {
                    a.craft_spear = true;
                    craft_cooldown = 8;
                    return true;
                }
                if (water_visible && !(is_night && water_d_m > 10.0f)) {
                    steer_(a, water_dx, water_dy);
                    // Bank-fishing: the env now registers near_water within 6 m
                    // (terrain pins the capsule ~5-6 m from the water centre),
                    // so press drink as soon as we're at the bank rather than
                    // demanding the unreachable 2 m DRINK_RANGE.
                    if (water_d_m < 6.5f) a.drink = true;
                    return true;
                }
                float qx, qy;
                if (!find_prey_(obs, qx, qy)) return false;
                float qd2  = qx*qx + qy*qy;
                float qd_m = std::sqrt(qd2) * VISION_RANGE;
                if (is_night && qd_m > 12.0f) return false;
                steer_(a, qx, qy);
                if (std::sqrt(qd2) < ATTACK_RANGE) a.attack = true;
                return true;
            };

            if (need_vit || need_pro) {
                // Fix the worse deficiency first.
                const bool vit_first =
                    need_vit && (!need_pro || vitamin <= protein);
                if (vit_first) {
                    if (try_forage())             return a;
                    if (need_pro && try_protein()) return a;
                } else {
                    if (need_pro && try_protein()) return a;
                    if (need_vit && try_forage())  return a;
                }
                // Nothing edible in vision: daytime bounded scout so the agent
                // goes LOOKING for food/prey instead of idling at home while a
                // bar bleeds to 0. Mirrors the Layer-7 final wander (random
                // heading refreshed every 32 ticks). Night -> defer to the
                // shelter-biased layers below (don't roam in the dark).
                if (!is_night) {
                    if ((tick & 31) == 0) {
                        float ang = std::uniform_real_distribution<float>(
                            0.0f, 6.2831853f)(rng);
                        dirx = std::cos(ang);
                        diry = std::sin(ang);
                    }
                    a.move_x = dirx;
                    a.move_y = diry;
                    return a;
                }
            }
        }

        // ---------- Layer 3: VITAL MAINTENANCE ----------
        // 3a) thirst moderate: only act if water close OR no other choice
        if (thirst < TH_LOW) {
            if (water_visible) {
                bool worth_going =
                    (water_d_m < WATER_FAR_M) ||
                    (thirst < 0.30f) ||
                    (!is_night && hunger > TH_LOW);
                bool too_risky_night = is_night && water_d_m > 10.0f;
                if (worth_going && !too_risky_night) {
                    steer_(a, water_dx, water_dy);
                    if (water_dnorm < DRINK_RANGE) a.drink = true;
                    return a;
                }
            }
            // fallback: if it's night, stay home & take the loss
            if (is_night && have_home_anchor) {
                steer_(a, home_dx, home_dy);
                return a;
            }
        }
        // 3b) hunger moderate
        if (hunger < TH_LOW) {
            float px, py;
            if (find_plant_(obs, px, py)) {
                float pd2 = px*px + py*py;
                float pd_m = std::sqrt(pd2) * VISION_RANGE;
                bool worth = (pd_m < 20.0f) || (hunger < 0.30f);
                bool too_risky_night = is_night && pd_m > 10.0f;
                if (worth && !too_risky_night) {
                    steer_(a, px, py);
                    if (std::sqrt(pd2) < EAT_RANGE) a.eat = true;
                    return a;
                }
            }
            if (is_night && have_home_anchor) {
                steer_(a, home_dx, home_dy);
                return a;
            }
        }

        // ---------- Layer 3.5: WARMTH-PREP (D-111 Stage3 Level-1) ----------
        // PROACTIVE clothing. Diag-1 (round_9): the clone almost never wears
        // clothes (token ~ 0) because the old clothing logic (Layer 6) only
        // fires at home, in daytime, with spare grass >= 3 -- yet grass is
        // spent on the build site and the night routine sleeps before reaching
        // Layer 6. The performance-limiting deaths are freeze deaths (temp -> 0
        // at night when unsheltered); a grass dress (cold_resist 0.10) damps
        // that drain. craft_grass_dress / wear_clothes are location-independent
        // (Environment.cpp: inventory-only), so we get dressed in the field
        // BEFORE the cold night, not only at home. Grass-dress only (Level-1);
        // never sets attack; adds no reward. Self-limiting: once worn,
        // worn_none is false so this layer stops firing.
        // D-112 (decision_round_10): clothing is a cheap COMPLEMENT to fire,
        // not the freeze fix (dress resist=0.10 only). To keep it from
        // competing with the core build (the r8 dilution lesson) and to keep
        // fire's bc_v11 attribution clean, WARMTH-PREP only dresses once the
        // home is already up. The dress now costs 1 grass (Inventory bugfix),
        // so >=1 grass (obs >0.005 under /CAPACITY norm) is enough to craft.
        {
            const float TEMP_PREP = 0.55f;  // body temp cooling below neutral
            const bool cold_risk = approaching_night || is_night
                                   || temp_v < TEMP_PREP;
            if (worn_none && cold_risk && home_complete) {
                // 3.5a) have a garment -> wear it (instant). Best ROI.
                if ((inv_dress > 0.005f || inv_cloak > 0.005f)
                    && wear_cooldown == 0) {
                    a.wear_clothes = true;
                    wear_cooldown = 8;
                    return a;
                }
                // 3.5b) have >=1 grass -> craft the dress now (instant).
                if (inv_g > 0.005f && inv_dress < 0.005f
                    && craft_cooldown == 0) {
                    a.craft_grass_dress = true;
                    craft_cooldown = 8;
                    return a;
                }
                // 3.5c) need grass -> grab nearest within reach, daytime, vitals
                // ok, no active site. One grass is enough -> 3.5b crafts -> wear.
                if (!is_night && !site_active
                    && hunger > TH_LOW && thirst > TH_LOW && inv_dress < 0.005f) {
                    float gx, gy;
                    if (find_resource_(obs, 2, gx, gy)) {
                        float gd2  = gx*gx + gy*gy;
                        float gd_m = std::sqrt(gd2) * VISION_RANGE;
                        if (gd_m < 12.0f) {
                            steer_(a, gx, gy);
                            if (std::sqrt(gd2) < COLLECT_RANGE) a.collect = true;
                            return a;
                        }
                    }
                }
            }
        }

        // ---------- Layer 3.6: FIRE (D-112 Stage3 Level-2) ----------
        // The campfire is the PRIMARY freeze fix (decision_round_10): unlike
        // the grass dress (resist 0.10, ~18s of survival) a lit fire actively
        // pulls body temperature back up within radius, directly offsetting
        // the 18/min night drain that kills agents who build a house but
        // still freeze outside it. It is a placed world entity, so it
        // sidesteps the CAPACITY==3 inventory block clothing ran into. Costs
        // 1 Wood per build/refuel; NO reward is attached (Goodhart redline).
        //
        // Unlike clothing this layer is deliberately NOT gated on
        // home_complete: the answer to "when/where do I light a fire" is
        // "whenever cold, wherever I am", so the oracle lights one the moment
        // it is cold and holds wood. fire_timer is the oracle's private fuel
        // estimate (no fire channel in obs); we (re)light a bit before it runs
        // out so warmth never lapses at night. A genuinely cold body
        // (temp_v < 0.35) forces a relight regardless of the estimate, which
        // also makes DAgger relabel freeze-death states with tend_fire.
        {
            const float TEMP_FIRE = 0.55f;
            const bool  cold_risk = approaching_night || is_night
                                    || temp_v < TEMP_FIRE;
            const bool  fuel_low  = fire_timer <= (FIRE_FUEL_TICKS / 4);
            const bool  body_cold = temp_v < 0.35f;
            // Protect the core build: never burn wood needed by an in-progress
            // home site (the r8 dilution lesson). Once the home is up (or none
            // is pending) fire is unrestricted, everywhere cold.
            if (!no_fire && cold_risk && (fuel_low || body_cold) && fire_cooldown == 0
                && !site_active) {
                // Have wood -> build a fire at my feet / refuel the nearby one.
                if (have_wood) {
                    a.tend_fire   = true;
                    fire_cooldown = 6;
                    fire_timer    = FIRE_FUEL_TICKS;   // assume a fresh load
                    return a;
                }
                // No wood but cold -> grab the nearest wood (daytime, vitals
                // ok, no active build competing) so the next pass can light it.
                if (!is_night && !site_active
                    && hunger > TH_LOW && thirst > TH_LOW) {
                    float wx, wy;
                    if (find_resource_(obs, 0, wx, wy)) {
                        float wd2  = wx*wx + wy*wy;
                        float wd_m = std::sqrt(wd2) * VISION_RANGE;
                        if (wd_m < 14.0f) {
                            steer_(a, wx, wy);
                            if (std::sqrt(wd2) < COLLECT_RANGE) a.collect = true;
                            return a;
                        }
                    }
                }
            }
        }

        // ---------- Layer 4: CONSTRUCTION (only if site exists & not done) ----------
        if (site_active) {
            // Have all required mats nearby -> deposit
            bool can_deposit_now =
                (have_wood  || !site_needs_w) &&
                (have_stone || !site_needs_s) &&
                (have_grass || !site_needs_g);
            if ((have_wood || have_grass || have_stone) && can_deposit_now) {
                if (home_d_norm < DEPOSIT_RANGE) {
                    a.move_x = 0.0f; a.move_y = 0.0f;
                    if (deposit_cooldown == 0) {
                        a.deposit_to_site = true;
                        deposit_cooldown = 3;
                    }
                    return a;
                }
                steer_(a, home_dx, home_dy);
                return a;
            }
            // Missing mats: collect the nearest needed kind
            float bx = 0.0f, by = 0.0f;
            float best = 1e9f;
            int   best_kind = -1;
            auto try_kind = [&](int kind, bool needed) {
                if (!needed) return;
                float dx, dy;
                if (!find_resource_(obs, kind, dx, dy)) return;
                float d2 = dx*dx + dy*dy;
                if (d2 < best) { best = d2; bx = dx; by = dy; best_kind = kind; }
            };
            try_kind(0, site_needs_w && !have_wood);
            try_kind(1, site_needs_s && !have_stone);
            try_kind(2, site_needs_g && !have_grass);
            if (best_kind >= 0) {
                steer_(a, bx, by);
                if (std::sqrt(best) < COLLECT_RANGE) a.collect = true;
                return a;
            }
            // No visible resource of needed kind: walk back near home & wait
            if (have_home_anchor) {
                steer_(a, home_dx, home_dy);
                return a;
            }
        }

        // ---------- Layer 5: SETTLEMENT ----------
        if (home_complete) {
            if (!at_home) {
                // Walk home, optionally pick something up along the way
                steer_(a, home_dx, home_dy);
                return a;
            }
            // We ARE at home (within 8 m) + vitals ok + home done.
            // D-102: night routine = SLEEP at the home core. The old settler
            // fell straight through to Layer 7 here, which let it keep
            // foraging within 12 m even after dark (unrealistic, and it never
            // demonstrated returning to center while already "home"). Now, at
            // night, actively walk back to the tight core and then rest in
            // place. The active return is the key training signal: it gives BC
            // a "drifted -> steer home" example at every distance inside the
            // home zone, which a frozen idle (old behaviour) never produced, so
            // the clone's small continuous-action drift had no demonstrated
            // correction and slowly walked it out of the coverage radius.
            if (is_night) {
                if (home_d_m > HOME_CORE_M) {
                    steer_(a, home_dx, home_dy);
                    return a;
                }
                a.move_x = 0.0f; a.move_y = 0.0f;  // sleep
                return a;
            }
            // Daytime at home: fall through to crafting (Layer 6) and the
            // short courtyard chores / idle (Layer 7) below.
        }

        // ---------- Layer 6: CRAFTING (only at home) ----------
        if (at_home && craft_cooldown == 0) {
            // 6a) no clothing + >=1 grass -> craft dress (1-grass recipe).
            if (worn_none && inv_g > 0.005f && inv_dress < 0.005f) {
                a.craft_grass_dress = true;
                craft_cooldown = 8;
                return a;
            }
            // 6b) have dress in inv but not worn -> wear
            if (worn_none && (inv_dress > 0.005f || inv_cloak > 0.005f)
                && wear_cooldown == 0) {
                a.wear_clothes = true;
                wear_cooldown = 8;
                return a;
            }
            // 6c) fur >= 2 (2/3 under /CAPACITY norm) + no cloak -> craft cloak
            if (inv_fur > 0.6f && inv_cloak < 0.005f) {
                a.craft_fur_cloak = true;
                craft_cooldown = 8;
                return a;
            }
            // 6d) wood >= 1 + stone >= 1 + no spear -> craft_spear
            if (!have_spear && have_wood && have_stone) {
                a.craft_spear = true;
                craft_cooldown = 8;
                return a;
            }
        }

        // ---------- Layer 7: EXPLORATION / TOP-UP ----------
        // At home + vitals ok + nothing to craft: short scouting walk for
        // spare materials, then come home. Cap at ~10m so we never drift far.
        if (at_home) {
            // pick a needed resource even though no site needs it (build buffer)
            float bx = 0.0f, by = 0.0f;
            float best = 1e9f;
            auto try_kind = [&](int kind, bool needed) {
                if (!needed) return;
                float dx, dy;
                if (!find_resource_(obs, kind, dx, dy)) return;
                float d_m = std::sqrt(dx*dx + dy*dy) * VISION_RANGE;
                if (d_m > 12.0f) return;  // too far, don't drift
                float d2 = dx*dx + dy*dy;
                if (d2 < best) { best = d2; bx = dx; by = dy; }
            };
            try_kind(0, inv_w < (3.0f / 20.0f));
            try_kind(1, inv_s < (3.0f / 20.0f));
            try_kind(2, inv_g < (3.0f / 20.0f));
            if (best < 1e9f) {
                steer_(a, bx, by);
                if (std::sqrt(best) < COLLECT_RANGE) a.collect = true;
                return a;
            }
            // 7b) Late stage farming: water nearby plot if any
            if (stage_late) {
                // (plot dx at obs[140]) - intentionally not wired in v1; future expansion.
            }
            // Default: idle in place. This is what creates bldg_ticks.
            a.move_x = 0.0f; a.move_y = 0.0f;
            return a;
        }

        // Final fallback: nothing else applies & we don't have a home anchor.
        // Wander to discover the world (will eventually trigger the auto-Shed
        // build at spawn or find resources).
        if ((tick & 31) == 0) {
            float ang = std::uniform_real_distribution<float>(0.0f, 6.2831853f)(rng);
            dirx = std::cos(ang);
            diry = std::sin(ang);
        }
        a.move_x = dirx;
        a.move_y = diry;
        return a;
    }
};

}  // namespace

// ---------------------------------------------------------------------------
// S4 PPO (decision_round_16 Step2): night-shelter potential for STRICT
// potential-based reward shaping F(s,s') = gamma*Phi(s') - Phi(s)
// (Ng/Harada/Russell 1999). Policy-invariant: only accelerates credit
// assignment for "be sheltered at night", never changes the optimal policy.
// This is NOT a direct nShel/enter-house reward (that would be Goodhart).
// Phi in [0,1]: at night, 1 when sheltered (in cave or within home core 8m),
// graded down to 0 by 20m; 0 by day. Reuses the obs indices the
// ScriptedSettler oracle uses (OBS_INSIDE_CAVE=9, OBS_DAY_COS=13,
// OBS_BLDG_DX/DY=130/131, VISION_RANGE=60).
static float phi_night_shelter(const dp::agent::ObservationBuilder::Vector& obs) {
    constexpr int   IDX_INSIDE_CAVE = 9;
    constexpr int   IDX_DAY_COS     = 13;
    constexpr int   IDX_BLDG_DX     = 130;
    constexpr int   IDX_BLDG_DY     = 131;
    constexpr float VISION_RANGE    = 60.0f;
    const float sun_h = -obs[IDX_DAY_COS];          // sun_height_normalized()
    if (sun_h >= -0.1f) return 0.0f;                // daytime -> no potential
    if (obs[IDX_INSIDE_CAVE] > 0.5f) return 1.0f;   // in a cave = sheltered
    const float dx = obs[IDX_BLDG_DX], dy = obs[IDX_BLDG_DY];
    const float d_norm = std::sqrt(dx * dx + dy * dy);
    if (d_norm <= 1e-4f) return 0.0f;               // no home anchor placed
    const float d_m = d_norm * VISION_RANGE;
    return std::clamp((20.0f - d_m) / 12.0f, 0.0f, 1.0f);  // 1 @<=8m -> 0 @20m
}

// round_19 Path B: daytime foraging potential to offset the new-bank
// food-death side-effect. Policy-invariant when summed with phi_night
// (Ng 1999 / Wiewiora 2003): Phi_total = Phi_night + Phi_day. This is
// NOT a direct food/collect reward (that would be Goodhart). Phi in
// [0,1]: by day (sun_h>+0.1), graded by proximity to the nearest ripe
// plant (obs 39..40 dx/dy, kind one-hot 42..45); 0 at night and when
// no ripe plant is in vision.
static float phi_day_forage(const dp::agent::ObservationBuilder::Vector& obs) {
    constexpr int   IDX_DAY_COS   = 13;
    constexpr int   IDX_PLANT0_DX = 39;
    constexpr int   IDX_PLANT0_DY = 40;
    constexpr int   IDX_PLANT0_K0 = 42;   // one-hot kind occupies 42..45
    constexpr float VISION_RANGE  = 60.0f;
    const float sun_h = -obs[IDX_DAY_COS];          // sun_height_normalized()
    if (sun_h <= 0.1f) return 0.0f;                 // not clearly daytime
    const float kindsum = obs[IDX_PLANT0_K0] + obs[IDX_PLANT0_K0 + 1]
                        + obs[IDX_PLANT0_K0 + 2] + obs[IDX_PLANT0_K0 + 3];
    if (kindsum < 0.5f) return 0.0f;                // no ripe plant in vision
    const float dx = obs[IDX_PLANT0_DX], dy = obs[IDX_PLANT0_DY];
    const float d_m = std::sqrt(dx * dx + dy * dy) * VISION_RANGE;
    return std::clamp((VISION_RANGE - d_m) / VISION_RANGE, 0.0f, 1.0f); // 1 @0m -> 0 @60m
}


int main(int argc, char** argv) {
    auto console = spdlog::stdout_color_mt("deep_protagonist_train");
    spdlog::set_default_logger(console);
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

    // CLI parsing (minimal hand-rolled, no extra deps)
    int      max_steps    = 10000;
    int      max_episodes = 0;            // 0 = unlimited
    uint64_t seed         = 12345;
    std::string policy    = "wander";
    std::string metrics_path;
    std::string save_path;                // checkpoint output (PPO only)
    int         save_every = 0;           // save every N updates (0=off)
    std::string load_path;                // resume from this checkpoint
    bool        no_update  = false;       // pure-eval: skip rollout collection and PPO update
    float       thirst_decay = 12.0f;      // D-053: blueprint line 60. D-041 used 8 as middle ground; with D-052 stack in place we need the full 12/min so drink stays a survival need across an episode.
    float       trigger_cost = 0.001f;     // EnvConfig.trigger_cost
    int         n_envs       = 1;          // D-078: VecEnv parallel envs (1 = legacy single env)
    bool        domain_rand  = false;      // 2026-06-01: re-roll world seed per episode (anti-overfit)
    // D-101 BC pipeline
    std::string record_demos;              // path to write (obs,action) demos (settler/scripted only)
    bool        demo_no_regen = false;     // disable in-building regen during demo recording
    bool        oracle_no_fire = false;    // D-114: suppress oracle fire layer (fire-free night DAgger)
    // D-102 DAgger: step the env with the LOADED clone's action (so we sample
    // the state distribution the clone actually visits at eval time) but record
    // the ORACLE (ScriptedSettler) action as the demo LABEL. This relabels the
    // clone's drift states with the correct expert action, fixing the camp-phase
    // covariate shift that plain BC cannot (the clone reaches house states
    // slightly off the oracle's distribution and wanders out). Requires
    // --policy ppo --load clone.pt --record-demos out.bin --dagger.
    bool        dagger = false;
    std::string bc_demos;                  // comma-separated demo files for --policy bc
    std::string bc_low_os_demos;           // subset of bc_demos using bc_low_os (night downweight)
    int         bc_low_os    = -1;         // <0 = unused
    std::string bc_out       = "bc.pt";    // BC output checkpoint
    int         bc_epochs    = 8;
    int         bc_batch     = 16;
    int         bc_bptt      = 64;
    float       bc_lr        = 1e-3f;
    float       bc_trig_w    = 5.0f;
    int         bc_min_eplen = 0;
    int         bc_max_eplen = 0;
    int         bc_build_os  = 0;
    int         bc_build_len = 50;
    int         bc_trig_os   = 0;
    int         bc_trig_pre  = 8;
    int         bc_trig_post = 12;
    bool        deterministic = false;   // PPO eval: use distribution mode (no sampling noise)
    // D-101 finetune: override PPO defaults from CLI (sentinel <0 = keep D-099
    // baseline). ent_coef=0.01 is tuned for FRESH init; when finetuning a BC
    // warm-start it over-rewards entropy and inflates the unbounded continuous
    // log_std, eroding the cloned behavior (entropy climbs 2.9->7.9, bldg
    // collapses). A small finetune ent_coef keeps PPO refining inside the BC
    // basin instead of exploring back out to the foraging attractor.
    float       ppo_ent_coef = -1.0f;
    float       ppo_lr       = -1.0f;
    int         ppo_critic_warmup = 0;   // D-101: value-only updates before policy training (BC->PPO)
    bool        ppo_mask_atk_fire = false;  // decision_round_17: mask attack(2)+fire(17)
    float       ppo_kl_coef       = 0.0f;   // decision_round_17: beta KL(pi||bc_v9)
    std::string ppo_kl_ref;                 // decision_round_17: reference ckpt (bc_v9.pt)
    float       ppo_target_kl     = 0.0f;   // decision_round_17: KL early-stop (0=off)
    float       ppo_night_shaping = 0.0f; // decision_round_16 Step2: coef for night-shelter PBRS (0=off)
    float       ppo_day_shaping   = 0.0f; // round_19 Path B: coef for daytime-forage PBRS (0=off)
    // decision_blueprint_nutrition (round_21): dual nutrition bars + hunting.
    bool        nutrition_enabled = false; // master switch (default off = old world)
    float       nutri_protein_decay = 20.0f;
    float       nutri_vitamin_decay = 20.0f;
    float       nutri_debuff_thresh = 0.30f;
    float       nutri_death_grace   = 8.0f;
    bool        ppo_mask_fire_only  = false; // mask fire only, leave attack open

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) {
                spdlog::error("missing value for {}", a);
                std::exit(2);
            }
            return std::string(argv[++i]);
        };
        if      (a == "--steps")      max_steps    = std::stoi(next());
        else if (a == "--episodes")   max_episodes = std::stoi(next());
        else if (a == "--seed")       seed         = std::stoull(next());
        else if (a == "--policy")     policy       = next();
        else if (a == "--metrics")    metrics_path = next();
        else if (a == "--save-path")  save_path    = next();
        else if (a == "--save-every") save_every   = std::stoi(next());
        else if (a == "--load")       load_path    = next();
        else if (a == "--no-update")  no_update    = true;
        else if (a == "--thirst-decay") thirst_decay = std::stof(next());
        else if (a == "--trigger-cost") trigger_cost = std::stof(next());
        else if (a == "--n-envs")       n_envs       = std::stoi(next());
        else if (a == "--domain-rand")  domain_rand  = true;
        else if (a == "--record-demos") record_demos = next();
        else if (a == "--demo-no-regen") demo_no_regen = true;
        else if (a == "--oracle-no-fire") oracle_no_fire = true;
        else if (a == "--dagger")        dagger        = true;
        else if (a == "--bc-demos")     bc_demos     = next();
        else if (a == "--bc-out")       bc_out       = next();
        else if (a == "--bc-epochs")    bc_epochs    = std::stoi(next());
        else if (a == "--bc-batch")     bc_batch     = std::stoi(next());
        else if (a == "--bc-bptt")      bc_bptt      = std::stoi(next());
        else if (a == "--bc-lr")        bc_lr        = std::stof(next());
        else if (a == "--bc-trigger-weight") bc_trig_w = std::stof(next());
        else if (a == "--bc-min-eplen") bc_min_eplen = std::stoi(next());
        else if (a == "--bc-max-eplen") bc_max_eplen = std::stoi(next());
        else if (a == "--bc-build-oversample") bc_build_os = std::stoi(next());
        else if (a == "--bc-build-len") bc_build_len = std::stoi(next());
        else if (a == "--bc-trigger-oversample") bc_trig_os = std::stoi(next());
        else if (a == "--bc-low-os")    bc_low_os    = std::stoi(next());
        else if (a == "--bc-low-os-demos") bc_low_os_demos = next();
        else if (a == "--bc-trig-pre")  bc_trig_pre  = std::stoi(next());
        else if (a == "--bc-trig-post") bc_trig_post = std::stoi(next());
        else if (a == "--deterministic") deterministic = true;
        else if (a == "--ent-coef")   ppo_ent_coef = std::stof(next());
        else if (a == "--lr")         ppo_lr       = std::stof(next());
        else if (a == "--critic-warmup") ppo_critic_warmup = std::stoi(next());
        else if (a == "--ppo-mask-atk-fire") ppo_mask_atk_fire = true;
        else if (a == "--ppo-kl-coef")   ppo_kl_coef   = std::stof(next());
        else if (a == "--ppo-kl-ref")    ppo_kl_ref    = next();
        else if (a == "--ppo-target-kl") ppo_target_kl = std::stof(next());
        else if (a == "--ppo-night-shaping") ppo_night_shaping = std::stof(next());
        else if (a == "--ppo-day-shaping")   ppo_day_shaping   = std::stof(next());
        else if (a == "--nutrition")            nutrition_enabled  = true;
        else if (a == "--nutri-protein-decay") nutri_protein_decay = std::stof(next());
        else if (a == "--nutri-vitamin-decay") nutri_vitamin_decay = std::stof(next());
        else if (a == "--nutri-debuff-thresh") nutri_debuff_thresh = std::stof(next());
        else if (a == "--nutri-death-grace")   nutri_death_grace   = std::stof(next());
        else if (a == "--ppo-mask-fire-only")  ppo_mask_fire_only  = true;
        else if (a == "-h" || a == "--help") {
            std::printf(
                "Usage: %s [--steps N] [--episodes N] [--seed S]\n"
                "       [--policy wander|noop|scripted|settler|ppo]\n"
                "       [--metrics path.jsonl]\n"
                "       [--save-path file.pt --save-every N]\n"
                "       [--load file.pt]\n"
                "       [--thirst-decay X --trigger-cost Y]\n"
                "       [--record-demos demos.bin [--demo-no-regen]]\n"
                "       [--policy bc --bc-demos a.bin,b.bin --bc-out bc.pt\n"
                "        --bc-epochs N --bc-batch B --bc-bptt T --bc-lr X\n"
                "        --bc-trigger-weight W]\n",
                argv[0]);
            return 0;
        } else {
            spdlog::warn("Unknown arg '{}', ignoring.", a);
        }
    }

    spdlog::info("================================================");
    spdlog::info(" deep_protagonist · headless trainer (S4-pre)");
    spdlog::info("================================================");
    spdlog::info("policy={}, steps={}, episodes_cap={}, seed={}",
                 policy, max_steps, max_episodes, seed);

    // D-101: Behavioral Cloning mode. Needs only the demo files, no env.
    if (policy == "bc") {
        dp::policy::BCConfig bc;
        bc.demo_paths     = split_csv(bc_demos);
        bc.out_path       = bc_out;
        bc.epochs         = bc_epochs;
        bc.batch_eps      = bc_batch;
        bc.bptt           = bc_bptt;
        bc.lr             = bc_lr;
        bc.trigger_weight = bc_trig_w;
        bc.min_eplen      = bc_min_eplen;
        bc.max_eplen      = bc_max_eplen;
        bc.build_oversample = bc_build_os;
        bc.build_len      = bc_build_len;
        bc.trigger_oversample = bc_trig_os;
        bc.low_os_demos       = split_csv(bc_low_os_demos);
        bc.low_trigger_oversample = bc_low_os;
        bc.trig_pre       = bc_trig_pre;
        bc.trig_post      = bc_trig_post;
        bc.seed           = static_cast<int>(seed & 0x7FFFFFFFULL);
        if (bc.demo_paths.empty()) {
            spdlog::error("--policy bc requires --bc-demos path[,path...]");
            return 2;
        }
        spdlog::info("BC: epochs={}, batch_eps={}, bptt={}, lr={}, trig_w={}, out={}",
                     bc.epochs, bc.batch_eps, bc.bptt, bc.lr,
                     bc.trigger_weight, bc.out_path);
        dp::policy::BCTrainer trainer(bc);
        return trainer.run() ? 0 : 1;
    }

    // Build EnvConfig from default.toml world settings, override seed.
    dp::env::EnvConfig ecfg;
    ecfg.world      = load_world_config("configs/default.toml");
    ecfg.world.seed = seed;
    ecfg.fixed_dt   = 1.0f / 30.0f;
    ecfg.thirst_decay_per_min = thirst_decay;
    ecfg.trigger_cost         = trigger_cost;
    ecfg.demo_no_regen        = demo_no_regen;
    // decision_blueprint_nutrition (round_21): plumb nutrition CLI into env.
    ecfg.nutrition_enabled     = nutrition_enabled;
    ecfg.protein_decay_per_min = nutri_protein_decay;
    ecfg.vitamin_decay_per_min = nutri_vitamin_decay;
    ecfg.nutri_debuff_thresh   = nutri_debuff_thresh;
    ecfg.nutri_death_grace_s   = nutri_death_grace;
    if (nutrition_enabled)
        spdlog::info("nutrition ON: protein_decay={}/min vitamin_decay={}/min "
                     "debuff<{:.0f}% death_grace={}s",
                     nutri_protein_decay, nutri_vitamin_decay,
                     nutri_debuff_thresh * 100.0f, nutri_death_grace);
    if (demo_no_regen) spdlog::info("D-101: demo_no_regen ON (in-building regen + r_alive bonus disabled)");
    spdlog::info("Env tunables: thirst_decay={}/min, trigger_cost={}/step",
                 thirst_decay, trigger_cost);

    spdlog::info("Building world: {}x{} cells @ {}m, max_height={}m",
                 ecfg.world.heightmap_cells_x, ecfg.world.heightmap_cells_y,
                 ecfg.world.cell_scale, ecfg.world.terrain_max_height);

    dp::env::Environment env(ecfg);
    spdlog::info("Environment built. obs_dim={}", env.observation_size());

    // Optional metrics file (one JSON object per line).
    std::FILE* mf = nullptr;
    if (!metrics_path.empty()) {
        mf = std::fopen(metrics_path.c_str(), "ab");
        if (!mf) {
            spdlog::error("Could not open metrics file '{}'", metrics_path);
            return 1;
        }
        spdlog::info("Metrics will append to: {}", metrics_path);
    }

    // D-101: optional demo recorder (settler / scripted only). Streams
    // (obs, cont_target, disc_idx, done) records; n_rec patched on close.
    std::FILE* demo_f = nullptr;
    uint64_t   demo_count = 0;
    constexpr uint32_t DEMO_OBS_DIM  = dp::policy::ActorCriticImpl::OBS_DIM;
    constexpr uint32_t DEMO_CONT_DIM = dp::policy::ActorCriticImpl::CONT_DIM;
    if (!record_demos.empty()) {
        demo_f = std::fopen(record_demos.c_str(), "wb");
        if (!demo_f) {
            spdlog::error("Could not open demo file '{}'", record_demos);
            return 1;
        }
        const char magic[4] = {'D', 'P', 'B', '1'};
        uint32_t ver = 1, od = DEMO_OBS_DIM, cd = DEMO_CONT_DIM;
        uint64_t n0 = 0;
        std::fwrite(magic, 1, 4, demo_f);
        std::fwrite(&ver, sizeof(uint32_t), 1, demo_f);
        std::fwrite(&od,  sizeof(uint32_t), 1, demo_f);
        std::fwrite(&cd,  sizeof(uint32_t), 1, demo_f);
        std::fwrite(&n0,  sizeof(uint64_t), 1, demo_f);
        spdlog::info("D-101: recording demos to '{}' (obs_dim={}, cont_dim={})",
                     record_demos, od, cd);
        if (dagger) {
            if (policy != "ppo" || load_path.empty()) {
                spdlog::error("--dagger needs --policy ppo --load <clone.pt> "
                              "(env steps with the clone, labels come from the oracle)");
                return 2;
            }
            spdlog::info("D-102 DAgger: stepping with clone '{}', labeling with "
                         "ScriptedSettler oracle", load_path);
        }
    }

    WanderPolicy wander(seed ^ 0xC0DEULL);
    ScriptedPolicy scripted(seed ^ 0xBEEFULL);
    ScriptedSettler settler(seed ^ 0xD100ULL);
    settler.no_fire = oracle_no_fire;
    if (oracle_no_fire) spdlog::info("D-114: oracle_no_fire ON -> ScriptedSettler suppresses tend_fire (fire-free relabel)");
    dp::agent::ObservationBuilder::Vector obs{};
    env.reset(obs);

    // PPO setup (only built if requested).
    std::unique_ptr<dp::policy::PPO> ppo;
    std::vector<dp::policy::Transition> rollout;
    dp::policy::PPOConfig pcfg;
    if (policy == "ppo") {
        pcfg.seed = static_cast<int>(seed & 0x7FFFFFFFULL);
        pcfg.n_envs = n_envs;  // D-078: VecEnv parallelism (PPO hidden_ shape [N, HIDDEN])
        if (ppo_ent_coef >= 0.0f) {
            pcfg.ent_coef = ppo_ent_coef;
            spdlog::info("D-101: ent_coef override {} (finetune; D-099 default 0.01)", ppo_ent_coef);
        }
        if (ppo_lr >= 0.0f) {
            pcfg.lr = ppo_lr;
            spdlog::info("D-101: lr override {} (finetune; D-099 default 3e-4)", ppo_lr);
        }
        pcfg.mask_atk_fire  = ppo_mask_atk_fire;
        pcfg.mask_fire_only = ppo_mask_fire_only;
        pcfg.kl_ref_coef    = ppo_kl_coef;
        pcfg.target_kl      = ppo_target_kl;
        ppo = std::make_unique<dp::policy::PPO>(pcfg);
        ppo->set_deterministic(deterministic);
        if (deterministic) spdlog::info("D-101: deterministic eval (mode action, no sampling noise)");
        rollout.reserve(pcfg.rollout_steps);
        if (!load_path.empty()) {
            ppo->load(load_path);
        }
        if (!ppo_kl_ref.empty() && ppo_kl_coef > 0.0f) {
            ppo->load_reference(ppo_kl_ref);
            spdlog::info("decision_round_17: KL anchor beta={} target_kl={}",
                         ppo_kl_coef, ppo_target_kl);
        }
    }
    int updates_since_save = 0;
    int total_updates      = 0;

    ActionStats astats;

    // D-043: Crafter-style cumulative milestone tracking. unlock_total[i] =
    // number of episodes in which milestone i was unlocked. The score is the
    // geometric mean of success rates: exp(mean(log(1 + s_i))) - 1, which
    // favors agents that unlock many milestones over those that grind one.
    std::array<int, dp::env::EpisodeMilestones::N> unlock_total{};
    int score_eps = 0;
    auto compute_score = [&]() {
        if (score_eps == 0) return 0.0;
        double s = 0.0;
        for (int i = 0; i < dp::env::EpisodeMilestones::N; ++i) {
            double rate = static_cast<double>(unlock_total[i]) / score_eps;
            s += std::log(1.0 + rate);
        }
        return std::exp(s / dp::env::EpisodeMilestones::N) - 1.0;
    };

    auto log_episode = [&](int episode_id,
                           const dp::agent::RewardSystem::EpisodeStats& s,
                           const dp::env::EpisodeMilestones& ms) {
        // Update cumulative counters BEFORE computing the score.
        for (int i = 0; i < dp::env::EpisodeMilestones::N; ++i) {
            if (ms.get(i)) ++unlock_total[i];
        }
        ++score_eps;
        double score = compute_score();
        spdlog::info("ep {}: steps={} R={:+.1f} unlocks={}/{} score={:.3f} "
                     "(alive={:+.1f} food={:+.1f} water={:+.1f} kills={:+.1f} "
                     "bites={:+.1f} death={:+.1f} vital={:+.1f} "
                     "collect={:+.1f} shelter={:+.1f} craft={:+.1f} "
                     "milestone={:+.1f} potential={:+.1f})",
                     episode_id, s.steps, s.total_reward,
                     ms.count(), dp::env::EpisodeMilestones::N, score,
                     s.r_alive, s.r_food, s.r_water, s.r_kills,
                     s.r_bites, s.r_death, s.r_vital,
                     s.r_collect, s.r_shelter, s.r_craft, s.r_milestone,
                     s.r_potential);
        if (mf) {
            double mv = astats.ticks > 0 ? astats.move_mag_sum / astats.ticks : 0.0;
            double yw = astats.ticks > 0 ? astats.yaw_abs_sum  / astats.ticks : 0.0;
            std::fprintf(mf,
                "{\"episode\":%d,\"steps\":%d,\"reward\":%.3f,"
                "\"r_alive\":%.3f,\"r_food\":%.3f,\"r_water\":%.3f,"
                "\"r_kills\":%.3f,\"r_bites\":%.3f,\"r_death\":%.3f,"
                "\"r_vital\":%.3f,\"r_collect\":%.3f,\"r_shelter\":%.3f,"
                "\"r_craft\":%.3f,\"r_milestone\":%.3f,\"r_potential\":%.3f,"
                "\"deaths\":%d,\"deaths_cold\":%d,\"deaths_food\":%d,"
                "\"deaths_protein\":%d,\"deaths_vitamin\":%d,"
                "\"protein\":%.1f,\"vitamin\":%.1f,"
                "\"policy\":\"%s\",\"seed\":%llu,"
                "\"mv\":%.3f,\"yw\":%.3f,\"score\":%.4f,\"unlocks\":[",
                episode_id, s.steps, s.total_reward,
                s.r_alive, s.r_food, s.r_water, s.r_kills,
                s.r_bites, s.r_death, s.r_vital,
                s.r_collect, s.r_shelter, s.r_craft, s.r_milestone,
                s.r_potential,
                env.vitals().deaths, env.vitals().deaths_cold,
                env.vitals().deaths_food,
                env.vitals().deaths_protein, env.vitals().deaths_vitamin,
                env.vitals().protein, env.vitals().vitamin, policy.c_str(),
                static_cast<unsigned long long>(seed),
                mv, yw, score);
            for (int i = 0; i < dp::env::EpisodeMilestones::N; ++i) {
                std::fprintf(mf, "%s%d", i == 0 ? "" : ",", ms.get(i) ? 1 : 0);
            }
            std::fprintf(mf, "],\"act\":[");
            for (int i = 0; i < ActionStats::N; ++i) {  // D-112: 18 slots, act[17]=fire token
                std::fprintf(mf, "%s%d", i == 0 ? "" : ",", astats.disc_count[i]);
            }
            // D-055: blueprint progression diagnostics. The episode reset
            // wipes EpisodeManager state but ProgressTracker is *persistent*
            // across episodes, so what we log here is the cumulative progress
            // the policy has carried since training started (or eval started).
            // D-091: settlement diagnostics. building_ticks = ticks inside
            // any completed building (any tier). night_total = ticks during
            // the dark half of the day cycle. night_shelter = night_total
            // ticks during which the agent was sheltered (lean-to OR
            // building OR cave). If "agent lives in the house" then
            // night_shelter / night_total -> 1 over training.
            int bldg_ticks = env.building_ticks();
            int night_total_ticks = env.night_ticks_total();
            int night_shelter_ticks = env.night_ticks_sheltered();
            std::fprintf(mf,
                "],\"stage\":%d,\"collect_total\":%d,\"explored_m\":%.1f,"
                "\"houses_built\":%d,\"sites_active\":%d,"
                "\"bldg_ticks\":%d,\"night_total\":%d,\"night_shelter\":%d}\n",
                static_cast<int>(env.progress().stage()),
                env.progress().resources_collected(),
                env.progress().total_explored(),
                env.progress().houses_built(),
                static_cast<int>(env.buildings().sites().size()),
                bldg_ticks, night_total_ticks, night_shelter_ticks);
            std::fflush(mf);
        }
        astats.reset();
    };

    auto t0 = std::chrono::steady_clock::now();
    int steps_done    = 0;
    int episodes_done = 0;

    // D-078: VecEnv path. When --n-envs N (N>1) is given AND policy=="ppo",
    // we build N parallel Environment instances and step them through
    // std::async, batching the policy forward into one [N, OBS_DIM] call.
    // Falls through to the legacy single-env loop when n_envs <= 1 or
    // when policy != "ppo".
    if (policy == "ppo" && n_envs > 1) {
        spdlog::info("D-078 VecEnv path: n_envs={}", n_envs);
        dp::env::VecEnv venv(ecfg, n_envs, seed ^ 0xFEEDULL);
        std::vector<dp::agent::ObservationBuilder::Vector> obs_batch;
        venv.reset_all(obs_batch);
        // Anti-overfit: per-episode world re-seeding stream (used iff --domain-rand).
        std::mt19937_64 dr_rng(seed ^ 0xDA7A5EEDULL);
        if (domain_rand)
            spdlog::info("2026-06-01: domain randomization ON (fresh world seed per episode)");

        // Per-env rollouts. Each fills to pcfg.rollout_steps before a
        // batched PPO update. Updates run sequentially across envs.
        std::vector<std::vector<dp::policy::Transition>> rollouts(n_envs);
        for (auto& r : rollouts) r.reserve(pcfg.rollout_steps);

        std::vector<dp::agent::AgentAction>      v_actions(n_envs);
        std::vector<std::vector<float>>          v_conts(n_envs);
        std::vector<int>                         v_disc(n_envs);
        std::vector<float>                       v_lp(n_envs), v_val(n_envs);
        std::vector<std::vector<float>>          v_hin(n_envs);
        std::vector<dp::env::StepResult>         v_sr(n_envs);

        while (steps_done < max_steps) {
            // Snapshot per-env GRU hiddens BEFORE the batched sample.
            for (int i = 0; i < n_envs; ++i) v_hin[i] = ppo->current_hidden_at(i);

            // Batched policy forward (single GPU call for N envs).
            ppo->sample_actions_batch(obs_batch, v_actions, v_conts,
                                      v_disc, v_lp, v_val);

            // Parallel env step (N std::async tasks).
            venv.step_all(v_actions, v_sr);
            steps_done += n_envs;

            for (int i = 0; i < n_envs; ++i) {
                astats.accumulate(v_actions[i]);

                if (!no_update) {
                    dp::policy::Transition tr;
                    tr.obs.assign(obs_batch[i].begin(), obs_batch[i].end());
                    tr.cont_act = std::move(v_conts[i]);
                    tr.disc_act = v_disc[i];
                    tr.h_in     = std::move(v_hin[i]);
                    float shaped_r = v_sr[i].reward;
                    if (ppo_night_shaping > 0.0f) {
                        float phi_s  = phi_night_shelter(obs_batch[i]);
                        float phi_sp = v_sr[i].done ? 0.0f
                                       : phi_night_shelter(v_sr[i].obs);
                        shaped_r += ppo_night_shaping
                                  * (pcfg.gamma * phi_sp - phi_s);
                    }
                    if (ppo_day_shaping > 0.0f) {
                        float dphi_s  = phi_day_forage(obs_batch[i]);
                        float dphi_sp = v_sr[i].done ? 0.0f
                                       : phi_day_forage(v_sr[i].obs);
                        shaped_r += ppo_day_shaping
                                  * (pcfg.gamma * dphi_sp - dphi_s);
                    }
                    tr.reward   = shaped_r;
                    tr.value    = v_val[i];
                    tr.log_prob = v_lp[i];
                    tr.done     = v_sr[i].done;
                    rollouts[i].push_back(std::move(tr));
                }

                if (v_sr[i].done) {
                    const auto ms = venv.env(i).milestones();
                    log_episode(v_sr[i].episode_id,
                                venv.env(i).last_episode_stats(), ms);
                    ++episodes_done;
                    if (domain_rand) {
                        // Fresh world (terrain + resource layout) every
                        // episode so the policy cannot memorize a fixed set
                        // of maps -> forces generalizable foraging/survival.
                        venv.reseed(i, dr_rng(), obs_batch[i]);
                    } else {
                        venv.reset(i, obs_batch[i]);
                    }
                    ppo->reset_hidden_at(i);
                } else {
                    obs_batch[i] = v_sr[i].obs;
                }
            }

            // Update when env-0's rollout fills. (Lockstep: every env
            // ticks once per macro-step so all reach pcfg.rollout_steps
            // at the same iteration.)
            if (!no_update &&
                static_cast<int>(rollouts[0].size()) >= pcfg.rollout_steps)
            {
                // Compute bootstrap V(s_T+1) for each env via a fresh
                // batched forward on the CURRENT obs_batch. This advances
                // hidden_ by one step in all rows, which is fine because
                // we are about to clear all rollouts.
                std::vector<dp::agent::AgentAction>  bs_a(n_envs);
                std::vector<std::vector<float>>      bs_c(n_envs);
                std::vector<int>                     bs_d(n_envs);
                std::vector<float>                   bs_lp(n_envs), bs_v(n_envs);
                ppo->sample_actions_batch(obs_batch, bs_a, bs_c, bs_d,
                                          bs_lp, bs_v);

                for (int i = 0; i < n_envs; ++i) {
                    float last_v = rollouts[i].back().done ? 0.0f : bs_v[i];
                    auto stats = ppo->update(rollouts[i], last_v,
                                             total_updates < ppo_critic_warmup);
                    ++total_updates;
                    ++updates_since_save;
                    if (i == 0) {
                        spdlog::info("PPO update #{:3d} (env0): pi={:+.4f}  "
                                     "v={:.4f}  ent={:.3f}  kl={:+.4f}  clip={:.2f}",
                                     total_updates,
                                     stats.policy_loss, stats.value_loss,
                                     stats.entropy,    stats.approx_kl,
                                     stats.clip_frac);
                    }
                    rollouts[i].clear();
                }

                if (save_every > 0 && !save_path.empty()
                    && updates_since_save >= save_every) {
                    ppo->save(save_path);
                    updates_since_save = 0;
                }
            }

            if (max_episodes > 0 && episodes_done >= max_episodes) break;
        }

        auto t1v = std::chrono::steady_clock::now();
        double elapsed_v = std::chrono::duration<double>(t1v - t0).count();
        double sps_v = elapsed_v > 0.0 ? steps_done / elapsed_v : 0.0;
        spdlog::info("VecEnv done: {} steps, {} episodes, {:.2f}s wall "
                     "({:.0f} steps/s, n_envs={})",
                     steps_done, episodes_done, elapsed_v, sps_v, n_envs);
        if (!save_path.empty()) ppo->save(save_path);
        if (mf) std::fclose(mf);
        return 0;
    }

    // Main loop. Two policy paths share the same step/done plumbing.
    std::vector<float> ppo_h_in_snapshot;   // h_in for current step
    while (steps_done < max_steps) {
        dp::agent::AgentAction action;
        std::vector<float> cont_act;
        int  disc_idx = dp::policy::ActorCriticImpl::NOOP_IDX;
        float lp = 0.0f, vv = 0.0f;
        if (policy == "noop") {
            // stand still
        } else if (policy == "wander") {
            action = wander.sample(ecfg.fixed_dt);
        } else if (policy == "scripted") {
            action = scripted.sample(obs, ecfg.fixed_dt);
        } else if (policy == "settler") {
            action = settler.sample(obs, ecfg.fixed_dt);
        } else if (policy == "ppo") {
            // Snapshot the GRU hidden BEFORE sample_action mutates it.
            // h_in is the value PPO::update replays during the recurrent
            // forward pass, so it has to match the hidden the GRU saw
            // on entry to this step.
            ppo_h_in_snapshot = ppo->current_hidden();
            action = ppo->sample_action(obs, cont_act, disc_idx, lp, vv);
        } else {
            spdlog::error("Unknown policy '{}'", policy);
            return 1;
        }

        astats.accumulate(action);
        auto sr = env.step(action);
        ++steps_done;

        // D-101: record this (obs, action) pair. obs still holds the
        // pre-step observation the policy acted on (it is overwritten by
        // sr.obs further down). done marks the episode boundary so BC can
        // replay episodes as sequences.
        if (demo_f) {
            // D-102 DAgger: env stepped with the clone's `action`, but the
            // recorded label is the oracle's action on this same (clone-visited)
            // obs. ScriptedSettler is purely obs-reactive (D-100: no episode
            // history), so querying it on an arbitrary state is valid.
            dp::agent::AgentAction label = dagger
                ? settler.sample(obs, ecfg.fixed_dt)
                : action;
            float cont3[3];
            action_to_cont(label, cont3);
            uint32_t didx = static_cast<uint32_t>(action_to_disc_idx(label));
            uint32_t done = sr.done ? 1u : 0u;
            std::fwrite(obs.data(), sizeof(float), DEMO_OBS_DIM, demo_f);
            std::fwrite(cont3, sizeof(float), DEMO_CONT_DIM, demo_f);
            std::fwrite(&didx, sizeof(uint32_t), 1, demo_f);
            std::fwrite(&done, sizeof(uint32_t), 1, demo_f);
            ++demo_count;
        }

        if (policy == "ppo" && !no_update) {
            dp::policy::Transition tr;
            tr.obs.assign(obs.begin(), obs.end());
            tr.cont_act = std::move(cont_act);
            tr.disc_act = disc_idx;
            tr.h_in     = std::move(ppo_h_in_snapshot);
            float shaped_r = sr.reward;
            if (ppo_night_shaping > 0.0f) {
                float phi_s  = phi_night_shelter(obs);
                float phi_sp = sr.done ? 0.0f : phi_night_shelter(sr.obs);
                shaped_r += ppo_night_shaping * (pcfg.gamma * phi_sp - phi_s);
            }
            if (ppo_day_shaping > 0.0f) {
                float dphi_s  = phi_day_forage(obs);
                float dphi_sp = sr.done ? 0.0f : phi_day_forage(sr.obs);
                shaped_r += ppo_day_shaping * (pcfg.gamma * dphi_sp - dphi_s);
            }
            tr.reward   = shaped_r;
            tr.value    = vv;
            tr.log_prob = lp;
            tr.done     = sr.done;
            rollout.push_back(std::move(tr));

            if (static_cast<int>(rollout.size()) >= pcfg.rollout_steps) {
                // Bootstrap value for last state if it didn't end in done.
                float last_v = 0.0f;
                if (!rollout.back().done) {
                    std::vector<float> dummy_c;
                    int   dummy_idx = 0;
                    float dummy_lp;
                    ppo->sample_action(sr.obs, dummy_c, dummy_idx,
                                       dummy_lp, last_v);
                }
                bool warming = total_updates < ppo_critic_warmup;
                auto stats = ppo->update(rollout, last_v, warming);
                ++total_updates;
                ++updates_since_save;
                spdlog::info("PPO update #{:3d}{}: pi={:+.4f}  v={:.4f}  "
                             "ent={:.3f}  kl={:+.4f}  clip={:.2f}",
                             total_updates, warming ? " [warmup]" : "",
                             stats.policy_loss, stats.value_loss,
                             stats.entropy,    stats.approx_kl,
                             stats.clip_frac);
                rollout.clear();

                if (save_every > 0 && !save_path.empty()
                    && updates_since_save >= save_every) {
                    ppo->save(save_path);
                    updates_since_save = 0;
                }
            }
        }

        obs = sr.obs;

        if (sr.done) {
            // D-043: snapshot milestones BEFORE reset() clears the flags.
            const auto ms = env.milestones();
            log_episode(sr.episode_id, env.last_episode_stats(), ms);
            ++episodes_done;
            if (max_episodes > 0 && episodes_done >= max_episodes) break;
            env.reset(obs);
            // New life -> reset GRU hidden so memory doesn't bleed across
            // episodes (matches reset of SpatialMemory inside Environment).
            if (policy == "ppo") ppo->reset_hidden();
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    double elapsed_s = std::chrono::duration<double>(t1 - t0).count();
    double steps_per_s = elapsed_s > 0.0 ? steps_done / elapsed_s : 0.0;
    spdlog::info("Done: {} steps, {} episodes, {:.2f}s wall ({:.0f} steps/s)",
                 steps_done, episodes_done, elapsed_s, steps_per_s);

    // Final save - always write a checkpoint at the end if save_path was
    // provided, regardless of the save_every cadence.
    if (policy == "ppo" && ppo && !save_path.empty()) {
        ppo->save(save_path);
    }
    if (mf) std::fclose(mf);
    if (demo_f) {
        // Patch n_rec in the header (offset 16 = 4 magic + 4 ver + 4 obs + 4 cont).
        std::fseek(demo_f, 16, SEEK_SET);
        std::fwrite(&demo_count, sizeof(uint64_t), 1, demo_f);
        std::fclose(demo_f);
        spdlog::info("D-101: wrote {} demo records to '{}'",
                     demo_count, record_demos);
    }
    return 0;
}
