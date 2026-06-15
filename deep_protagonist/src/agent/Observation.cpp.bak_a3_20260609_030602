#include "agent/Observation.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace dp::agent {

namespace {

inline float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

inline float clamp_norm(float v) {
    if (v < -1.0f) return -1.0f;
    if (v >  1.0f) return 1.0f;
    return v;
}

// Pick the K closest entries (in 2D distance from anchor) into out_idx,
// padded with -1 for empty slots. selector returns true if the entry is
// "alive" (e.g. plant ripe, animal not Dead, resource available).
template <typename T, typename Pos, typename Sel>
void k_nearest(const std::vector<T>& items,
               float ax, float ay,
               int K,
               Pos pos,
               Sel selector,
               int* out_idx) {
    // small-K selection sort over O(N) -> O(N*K)
    std::vector<float> best_d2(K, std::numeric_limits<float>::infinity());
    for (int k = 0; k < K; ++k) out_idx[k] = -1;
    for (int i = 0; i < int(items.size()); ++i) {
        if (!selector(items[i])) continue;
        float x, y;
        pos(items[i], x, y);
        float dx = x - ax, dy = y - ay;
        float d2 = dx*dx + dy*dy;
        // insert if smaller than the worst current pick
        for (int k = 0; k < K; ++k) {
            if (d2 < best_d2[k]) {
                // shift right
                for (int m = K - 1; m > k; --m) {
                    best_d2[m] = best_d2[m-1];
                    out_idx[m] = out_idx[m-1];
                }
                best_d2[k] = d2;
                out_idx[k] = i;
                break;
            }
        }
    }
}

// Helper: write (dx,dy,dz, one_hot_kind) into [out + offset], using
// VISION_RANGE for normalisation.
inline void write_relative(float* out, int n_kinds, int kind_idx,
                           float dx, float dy, float dz) {
    out[0] = clamp_norm(dx / ObservationBuilder::VISION_RANGE);
    out[1] = clamp_norm(dy / ObservationBuilder::VISION_RANGE);
    out[2] = clamp_norm(dz / ObservationBuilder::VISION_RANGE);
    for (int k = 0; k < n_kinds; ++k) {
        out[3 + k] = (k == kind_idx) ? 1.0f : 0.0f;
    }
}

inline void write_zero_slot(float* out, int n_kinds) {
    out[0] = 0.0f; out[1] = 0.0f; out[2] = 0.0f;
    for (int k = 0; k < n_kinds; ++k) out[3 + k] = 0.0f;
}

}  // namespace

void ObservationBuilder::compose(const dp::world::World&         world,
                                 const CapsuleAgent&              agent,
                                 const VitalState&                vital_state,
                                 const dp::world::Daylight&       day,
                                 const dp::world::PlantSystem&    plants,
                                 const dp::world::AnimalSystem&   animals,
                                 const dp::world::ResourceSystem& resources,
                                 const Inventory&                 inventory,
                                 const Shelter&                   shelter,
                                 const EpisodeManager&            episodes,
                                 const ProgressTracker&           progress,
                                 const dp::world::BuildingSystem& buildings,
                                 const Wardrobe&                  wardrobe,
                                 const dp::world::FarmingSystem&  farming,
                                 const TameSystem&                tame,
                                 dp::world::BuildingType          selected_building_type,
                                 const SpatialMemory&             spatial,
                                 float                            rain_intensity,
                                 float                            wetness,
                                 Vector&                          out) {
    std::memset(out.data(), 0, sizeof(float) * SIZE);
    const auto& s = agent.state();
    int p = 0;

    // ---- vital (4) ----
    out[p++] = clamp01(vital_state.hunger      / 100.0f);
    out[p++] = clamp01(vital_state.thirst      / 100.0f);
    out[p++] = clamp01(vital_state.stamina     / 100.0f);
    out[p++] = clamp01(vital_state.temperature / 100.0f);

    // ---- self (7) ----
    out[p++] = clamp_norm(s.vel_x / MAX_VEL);
    out[p++] = clamp_norm(s.vel_y / MAX_VEL);
    out[p++] = clamp_norm(s.vel_z / MAX_VEL);
    out[p++] = std::sin(agent.yaw());
    out[p++] = std::cos(agent.yaw());
    out[p++] = agent.inside_cave() ? 1.0f : 0.0f;
    out[p++] = s.on_ground         ? 1.0f : 0.0f;

    // ---- episode (1) ----
    // We don't have direct access to max_seconds here, so we infer the
    // ratio from EpisodeManager's elapsed and the safe upper bound 600s.
    out[p++] = clamp01(episodes.elapsed_s() / 300.0f);

    // ---- day (2) ----
    float ang = day.t() * 6.2831853f;
    out[p++] = std::sin(ang);
    out[p++] = std::cos(ang);

    // ---- terrain patch (5x5) ----
    float my_z = world.surface_height(s.pos_x, s.pos_y);
    int half = TERRAIN_PATCH / 2;
    for (int j = -half; j <= half; ++j) {
        for (int i = -half; i <= half; ++i) {
            float wx = s.pos_x + i * TERRAIN_STRIDE;
            float wy = s.pos_y + j * TERRAIN_STRIDE;
            float h = world.surface_height(wx, wy);
            out[p++] = clamp_norm((h - my_z) / MAX_HEIGHT_REL);
        }
    }

    // ---- 4 nearest plants ----
    {
        int idx[K_PLANTS];
        k_nearest(plants.plants(), s.pos_x, s.pos_y, K_PLANTS,
                  [](const dp::world::PlantInstance& pl, float& x, float& y) {
                      x = pl.x; y = pl.y;
                  },
                  [](const dp::world::PlantInstance& pl) { return pl.ripe; },
                  idx);
        for (int k = 0; k < K_PLANTS; ++k) {
            if (idx[k] < 0) {
                write_zero_slot(out.data() + p, 4);
            } else {
                const auto& pl = plants.plants()[idx[k]];
                write_relative(out.data() + p, 4,
                               int(pl.kind),
                               pl.x - s.pos_x,
                               pl.y - s.pos_y,
                               pl.z - s.pos_z);
            }
            p += 7;
        }
    }

    // ---- 4 nearest animals (slot 0 = guaranteed nearest huntable prey) ----
    // D-124: slot 0 is a dedicated rabbit/deer channel, mirroring the D-048
    // wolf slot. The generic "K nearest of any kind" fill let fish schools
    // (5-9/school) and wolf packs crowd out the nearest prey, so the policy
    // went blind to its hunt target -- AND phi_hunt_progress (which reads
    // these same slots) saw no prey, zeroing the approach-shaping gradient.
    // Pinning the nearest prey to slot 0 keeps it visible out to VISION_RANGE
    // so the agent can navigate to it and the shaping pull stays alive. The
    // remaining slots are still the nearest animals of any kind (excluding the
    // prey already in slot 0). Slot count / dims are unchanged (foundation).
    {
        const auto& av = animals.animals();
        int idx[K_ANIMALS];
        for (int k = 0; k < K_ANIMALS; ++k) idx[k] = -1;

        int prey1[1] = {-1};
        k_nearest(av, s.pos_x, s.pos_y, 1,
                  [](const dp::world::AnimalInstance& a, float& x, float& y) {
                      x = a.pos_x; y = a.pos_y;
                  },
                  [](const dp::world::AnimalInstance& a) {
                      return a.state != dp::world::AnimalState::Dead &&
                             (a.kind == dp::world::AnimalKind::Rabbit ||
                              a.kind == dp::world::AnimalKind::Deer);
                  },
                  prey1);
        idx[0] = prey1[0];
        const int slot0 = idx[0];

        int rest[K_ANIMALS];
        k_nearest(av, s.pos_x, s.pos_y, K_ANIMALS,
                  [](const dp::world::AnimalInstance& a, float& x, float& y) {
                      x = a.pos_x; y = a.pos_y;
                  },
                  [&av, slot0](const dp::world::AnimalInstance& a) {
                      if (a.state == dp::world::AnimalState::Dead) return false;
                      if (slot0 >= 0 && &a == &av[slot0]) return false;
                      return true;
                  },
                  rest);
        int r = 0;
        for (int k = 1; k < K_ANIMALS; ++k) idx[k] = rest[r++];

        for (int k = 0; k < K_ANIMALS; ++k) {
            if (idx[k] < 0) {
                write_zero_slot(out.data() + p, 5);
            } else {
                const auto& a = animals.animals()[idx[k]];
                write_relative(out.data() + p, 5,
                               int(a.kind),
                               a.pos_x - s.pos_x,
                               a.pos_y - s.pos_y,
                               a.pos_z - s.pos_z);
            }
            p += 8;
        }
    }

    // ---- 3 nearest resources ----
    {
        int idx[K_RESOURCES];
        k_nearest(resources.resources(), s.pos_x, s.pos_y, K_RESOURCES,
                  [](const dp::world::ResourceInstance& r, float& x, float& y) {
                      x = r.x; y = r.y;
                  },
                  [](const dp::world::ResourceInstance& r) { return r.available; },
                  idx);
        for (int k = 0; k < K_RESOURCES; ++k) {
            if (idx[k] < 0) {
                write_zero_slot(out.data() + p, 3);
            } else {
                const auto& r = resources.resources()[idx[k]];
                write_relative(out.data() + p, 3,
                               int(r.kind),
                               r.x - s.pos_x,
                               r.y - s.pos_y,
                               r.z - s.pos_z);
            }
            p += 6;
        }
    }

    // ---- inventory counts (4) ----
    out[p++] = inventory.num_of(ItemKind::Wood)  / float(Inventory::CAPACITY);
    out[p++] = inventory.num_of(ItemKind::Stone) / float(Inventory::CAPACITY);
    out[p++] = inventory.num_of(ItemKind::Grass) / float(Inventory::CAPACITY);
    out[p++] = inventory.num_of(ItemKind::Spear) / float(Inventory::CAPACITY);

    // ---- shelter (3) ----
    if (shelter.placed()) {
        out[p++] = 1.0f;
        out[p++] = clamp_norm((shelter.x() - s.pos_x) / VISION_RANGE);
        out[p++] = clamp_norm((shelter.y() - s.pos_y) / VISION_RANGE);
    } else {
        out[p++] = 0.0f;
        out[p++] = 0.0f;
        out[p++] = 0.0f;
    }

    // ---- progress stage one-hot (3) ----
    int stage_idx = static_cast<int>(progress.stage());
    for (int i = 0; i < 3; ++i) out[p++] = (i == stage_idx) ? 1.0f : 0.0f;

    // ---- progress norms (3) ----
    out[p++] = clamp01(progress.resources_collected()
                       / float(progress.mid_collect_threshold > 0
                               ? progress.mid_collect_threshold : 1));
    out[p++] = clamp01(progress.total_explored()
                       / (progress.mid_explore_threshold > 0
                          ? progress.mid_explore_threshold : 1.0f));
    out[p++] = clamp01(progress.houses_built() / 5.0f);

    // ---- home anchor (3) ----
    // D-096: replaced "nearest building" with "first completed building" so
    // PPO gets a STABLE spatial anchor across the whole episode. D-094 and
    // D-095 both showed PPO building the auto-Shed early but then unlearning
    // shelter use, partly because the previous "nearest" signal kept moving
    // (auto-Shed completes -> nearest is then far+pending -> obs flips
    // around). With insertion order = completion order, buildings[0] is
    // always the agent's first home (or the auto-placed Shed at spawn after
    // it finishes). This gives PPO a real "compass home" feature, matching
    // the user's blueprint requirement (remember the home's direction /
    // run back home at night).
    // Fallback to the first pending site (the auto-placed Shed) before any
    // completion, so the agent still has direction-to-future-home from tick 0.
    {
        bool   have = false;
        float  hx = 0.0f, hy = 0.0f;
        float  completed_flag = 0.0f;
        if (!buildings.buildings().empty()) {
            const auto& home = buildings.buildings().front();
            hx = home.x; hy = home.y;
            have = true;
            completed_flag = 1.0f;
        } else if (!buildings.sites().empty()) {
            const auto& site0 = buildings.sites().front();
            hx = site0.x; hy = site0.y;
            have = true;
            completed_flag = site0.completed ? 1.0f : 0.0f;
        }
        if (have) {
            out[p++] = clamp_norm((hx - s.pos_x) / VISION_RANGE);
            out[p++] = clamp_norm((hy - s.pos_y) / VISION_RANGE);
            out[p++] = completed_flag;
        } else {
            out[p++] = 0.0f; out[p++] = 0.0f; out[p++] = 0.0f;
        }
    }

    // ---- inventory late items (4) ----
    out[p++] = inventory.num_of(ItemKind::Fur)        / float(Inventory::CAPACITY);
    out[p++] = inventory.num_of(ItemKind::GrassDress) / float(Inventory::CAPACITY);
    out[p++] = inventory.num_of(ItemKind::FurCloak)   / float(Inventory::CAPACITY);
    out[p++] = inventory.num_of(ItemKind::Seed)       / float(Inventory::CAPACITY);

    // ---- worn one-hot (3) ----
    out[p++] = (wardrobe.worn() == ItemKind::Count)      ? 1.0f : 0.0f;
    out[p++] = (wardrobe.worn() == ItemKind::GrassDress) ? 1.0f : 0.0f;
    out[p++] = (wardrobe.worn() == ItemKind::FurCloak)   ? 1.0f : 0.0f;

    // ---- nearest farm plot (4: dx, dy, growth, water) ----
    {
        float best_d2 = std::numeric_limits<float>::infinity();
        int   best_idx = -1;
        for (int i = 0; i < int(farming.plots().size()); ++i) {
            const auto& fp = farming.plots()[i];
            if (fp.harvested) continue;
            float dx = fp.x - s.pos_x, dy = fp.y - s.pos_y;
            float d2 = dx*dx + dy*dy;
            if (d2 < best_d2) { best_d2 = d2; best_idx = i; }
        }
        if (best_idx >= 0) {
            const auto& fp = farming.plots()[best_idx];
            out[p++] = clamp_norm((fp.x - s.pos_x) / VISION_RANGE);
            out[p++] = clamp_norm((fp.y - s.pos_y) / VISION_RANGE);
            out[p++] = clamp01(fp.growth);
            out[p++] = clamp01(fp.water_level);
        } else {
            out[p++] = 0.0f; out[p++] = 0.0f; out[p++] = 0.0f; out[p++] = 0.0f;
        }
    }

    // ---- tame follower count (1) ----
    out[p++] = clamp01(tame.follower_count() / 4.0f);

    // ====================== S4.0 P0 additions ============================
    //
    // The blocks below sit *after* the original S3.10 layout so older
    // checkpoints stay aligned through 0..144 - if anyone wants to load a
    // pre-S4.0 .pt they'd still get the right vital/animals/etc, only the
    // new tail needs zero-init.

    // ---- nearest water cell (3: dx_norm, dy_norm, dist_norm) -----------
    // Scan a coarse grid of biome cells around the agent within
    // VISION_RANGE; pick the closest River or Swamp cell. Done in metres
    // so it works regardless of heightmap cell_scale.
    {
        const float step = 8.0f;
        float best_d2  = std::numeric_limits<float>::infinity();
        float best_dx  = 0.0f, best_dy = 0.0f;
        for (float dy = -VISION_RANGE; dy <= VISION_RANGE; dy += step) {
            for (float dx = -VISION_RANGE; dx <= VISION_RANGE; dx += step) {
                float wx = s.pos_x + dx, wy = s.pos_y + dy;
                auto b = world.biome_at(wx, wy);
                if (b != dp::world::Biome::River
                 && b != dp::world::Biome::Swamp) continue;
                float d2 = dx*dx + dy*dy;
                if (d2 < best_d2) { best_d2 = d2; best_dx = dx; best_dy = dy; }
            }
        }
        if (std::isfinite(best_d2)) {
            float dist = std::sqrt(best_d2);
            out[p++] = clamp_norm(best_dx / VISION_RANGE);
            out[p++] = clamp_norm(best_dy / VISION_RANGE);
            out[p++] = clamp01(dist / VISION_RANGE);
        } else {
            out[p++] = 0.0f; out[p++] = 0.0f; out[p++] = 1.0f;  // "no water in sight"
        }
    }

    // ---- D-048 nearest wolf (3: dx_norm, dy_norm, dist_norm) -----------
    // K_ANIMALS picks the 4 nearest creatures regardless of kind; if 4
    // rabbits are closer than the wolf, PPO loses sight of its biggest
    // threat. This dedicated slot guarantees the predator stays in obs
    // until VISION_RANGE so the policy can learn 'see wolf -> flee'.
    // Empty slot encodes "no wolf in sight" as dx=dy=0, dist=1.0
    // (mirrors the nearest_water convention right above).
    {
        float best_d2  = std::numeric_limits<float>::infinity();
        float best_dx  = 0.0f;
        float best_dy  = 0.0f;
        bool  any_wolf = false;
        const float R2 = VISION_RANGE * VISION_RANGE;
        for (const auto& a : animals.animals()) {
            if (a.kind != dp::world::AnimalKind::Wolf) continue;
            if (a.state == dp::world::AnimalState::Dead) continue;
            float dx = a.pos_x - s.pos_x;
            float dy = a.pos_y - s.pos_y;
            float d2 = dx*dx + dy*dy;
            if (d2 > R2) continue;
            if (d2 < best_d2) {
                best_d2  = d2;
                best_dx  = dx;
                best_dy  = dy;
                any_wolf = true;
            }
        }
        if (any_wolf) {
            float dist = std::sqrt(best_d2);
            out[p++] = clamp_norm(best_dx / VISION_RANGE);
            out[p++] = clamp_norm(best_dy / VISION_RANGE);
            out[p++] = clamp01(dist / VISION_RANGE);
        } else {
            out[p++] = 0.0f; out[p++] = 0.0f; out[p++] = 1.0f;
        }
    }

    // ---- biome one-hot at agent's feet (N_BIOMES) ----------------------
    {
        auto b = world.biome_at(s.pos_x, s.pos_y);
        int  idx = static_cast<int>(b);
        for (int i = 0; i < N_BIOMES; ++i) {
            out[p++] = (i == idx) ? 1.0f : 0.0f;
        }
    }

    // ---- selected building type one-hot (N_BUILDING_TYPES) -------------
    {
        int idx = static_cast<int>(selected_building_type);
        for (int i = 0; i < N_BUILDING_TYPES; ++i) {
            out[p++] = (i == idx) ? 1.0f : 0.0f;
        }
    }

    // ---- nearest active construction site remaining materials (3) ------
    // Picks the closest non-completed site and reports how much more
    // (wood/stone/grass) it needs to finish. Normalised by 20 since that
    // is the largest recipe component (BigHouse). 0 = "no site / done".
    {
        const auto& sites = buildings.sites();
        float best_d2 = std::numeric_limits<float>::infinity();
        int   best_idx = -1;
        for (int i = 0; i < int(sites.size()); ++i) {
            if (sites[i].completed) continue;
            float dx = sites[i].x - s.pos_x, dy = sites[i].y - s.pos_y;
            float d2 = dx*dx + dy*dy;
            if (d2 < best_d2) { best_d2 = d2; best_idx = i; }
        }
        if (best_idx >= 0) {
            const auto& site   = sites[best_idx];
            const auto& recipe = dp::world::building_recipe(site.type);
            int w_left = std::max(0, recipe.wood_required  - site.wood_deposited);
            int s_left = std::max(0, recipe.stone_required - site.stone_deposited);
            int g_left = std::max(0, recipe.grass_required - site.grass_deposited);
            out[p++] = clamp01(w_left / 20.0f);
            out[p++] = clamp01(s_left / 20.0f);
            out[p++] = clamp01(g_left / 20.0f);
        } else {
            out[p++] = 0.0f; out[p++] = 0.0f; out[p++] = 0.0f;
        }
    }

    // ---- SpatialMemory window (8x8x6 = 384) ----------------------------
    // Cognitive map read-out, gives PPO the equivalent of "what I've seen
    // within ~64 m of myself this life". Written into out at offset p
    // first (we'll bump p afterwards) since SpatialMemory::read_window
    // takes a plain float* and fills WINDOW_DIM consecutive floats.
    spatial.read_window(s.pos_x, s.pos_y, out.data() + p + 19);
    // Reserve the 384 slots; we still need to fill last_action *before*
    // them (slots are contiguous in the layout). Do that now.
    int spatial_offset_save = p + 19;
    (void)spatial_offset_save;  // already written, p advances at the end

    // ---- last_action (3 continuous + 16 discrete = 19) -----------------
    // Standard RL trick: feed the previous action back into the obs so a
    // recurrent or feed-forward policy can maintain intent / momentum.
    {
        const AgentAction& la = agent.last_action();
        out[p++] = clamp_norm(la.move_x);
        out[p++] = clamp_norm(la.move_y);
        out[p++] = clamp_norm(la.yaw_rate / 3.14159265f);   // rad/s -> ~unit
        // Discrete flags in the same order as ACTION_DISCRETE_DIM (16):
        out[p++] = la.eat                  ? 1.0f : 0.0f;
        out[p++] = la.drink                ? 1.0f : 0.0f;
        out[p++] = la.attack               ? 1.0f : 0.0f;
        out[p++] = la.collect              ? 1.0f : 0.0f;
        out[p++] = la.place_shelter        ? 1.0f : 0.0f;
        out[p++] = la.craft_spear          ? 1.0f : 0.0f;
        out[p++] = la.sleep                ? 1.0f : 0.0f;
        out[p++] = la.craft_grass_dress    ? 1.0f : 0.0f;
        out[p++] = la.craft_fur_cloak      ? 1.0f : 0.0f;
        out[p++] = la.wear_clothes         ? 1.0f : 0.0f;
        out[p++] = la.place_blueprint      ? 1.0f : 0.0f;
        out[p++] = la.cycle_building_type  ? 1.0f : 0.0f;
        out[p++] = la.deposit_to_site      ? 1.0f : 0.0f;
        out[p++] = la.plant_seed           ? 1.0f : 0.0f;
        out[p++] = la.water_plot           ? 1.0f : 0.0f;
        out[p++] = la.feed_tame            ? 1.0f : 0.0f;
    }

    // Advance p past the SpatialMemory window we already wrote above.
    p += SpatialMemory::WINDOW_DIM;

    // ---- nutrition bars (2) — decision_blueprint_nutrition §3a -----------
    // protein/vitamin normalised /100, appended last (indices 567, 568).
    // When nutrition is OFF the bars stay at 100 so these read 1.0/1.0 (a
    // constant), and a warm-started bc_v9 (zero-padded encoder columns) is
    // mathematically blind to them — behaviour identical to the old world.
    out[p++] = clamp01(vital_state.protein / 100.0f);
    out[p++] = clamp01(vital_state.vitamin / 100.0f);

    // ---- S5 world realism: environment direct-sense (2) ------------------
    // Appended at the VERY END (indices 569, 570) so 0..568 stay byte-stable
    // and a 569-dim v6/BC first layer warm-starts by zero-padding these 2 new
    // encoder columns (surgery). These let the protagonist FEEL the new world:
    //   [569] ambient climate target /100 — the temperature its body is being
    //         pulled toward right now (0=cold: night/mountain, 1=hot: desert
    //         daytime, 0.5=mild). Lets the policy pre-empt freezing/overheating
    //         (build fire / wear clothes) BEFORE temperature actually drops,
    //         matching the VitalSystem drift target exactly.
    //   [570] terrain slope under the feet = |grad h| (rise/run, capped 1.0),
    //         the same quantity that now costs extra stamina + slows movement.
    {
        float sun_h = day.sun_height_normalized();
        auto  bm    = world.biome_at(s.pos_x, s.pos_y);
        bool is_night   = sun_h < -0.1f;
        bool mountain   = (bm == dp::world::Biome::Mountain);
        bool desert_day = (bm == dp::world::Biome::Desert) && (sun_h > 0.4f);
        float ambient = 0.5f;               // mild → drift toward 50/100
        if (is_night || mountain) ambient = 0.0f;   // cold drift toward 0
        else if (desert_day)      ambient = 1.0f;   // hot  drift toward 100
        out[p++] = ambient;

        float gdx = 0.0f, gdy = 0.0f;
        world.heightmap().sample_gradient(s.pos_x, s.pos_y, gdx, gdy);
        float sl = std::sqrt(gdx * gdx + gdy * gdy);
        out[p++] = sl > 1.0f ? 1.0f : sl;
    }

    // ---- Stage W1 world coherence: weather direct-sense (2) --------------
    // [571] global rain intensity (sky), [572] the agent's own wetness. Both
    // 0 when weather is disabled, so a warm-started (zero-padded) first layer
    // is blind to them and the old world behaves identically. Lets the policy
    // FEEL a shower coming and that it is getting soaked, so it can re-light a
    // fire / take cover / wear clothes before the cold drain bites.
    out[p++] = clamp01(rain_intensity);
    out[p++] = clamp01(wetness);

    // ---- D-122 one-pot tech-tree: reserved sense block (9 slots) ---------
    // Locked in now; each reads 0 until its rung's system lands, so a
    // warm-started champion (zero-padded encoder cols) is blind to them and
    // the old world behaves byte-identically. RUNG code will replace these
    // zeros with real signals (near_cookfire, ore_in_reach, has_axe, ...).
    for (int i = 0; i < OBS_TECH_RESERVED; ++i) out[p++] = 0.0f;

    // p should equal SIZE here.
}

}  // namespace dp::agent
