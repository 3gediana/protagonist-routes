#include "agent/Observation.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

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

    // ---- 4 nearest animals ----
    {
        int idx[K_ANIMALS];
        k_nearest(animals.animals(), s.pos_x, s.pos_y, K_ANIMALS,
                  [](const dp::world::AnimalInstance& a, float& x, float& y) {
                      x = a.pos_x; y = a.pos_y;
                  },
                  [](const dp::world::AnimalInstance& a) {
                      return a.state != dp::world::AnimalState::Dead;
                  },
                  idx);
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

    // ---- nearest building (3) ----
    {
        float best_d2 = std::numeric_limits<float>::infinity();
        int   best_idx = -1;
        bool  best_completed = false;
        for (int i = 0; i < int(buildings.sites().size()); ++i) {
            const auto& b = buildings.sites()[i];
            float dx = b.x - s.pos_x;
            float dy = b.y - s.pos_y;
            float d2 = dx*dx + dy*dy;
            if (d2 < best_d2) {
                best_d2 = d2;
                best_idx = i;
                best_completed = b.completed;
            }
        }
        for (int i = 0; i < int(buildings.buildings().size()); ++i) {
            const auto& b = buildings.buildings()[i];
            float dx = b.x - s.pos_x;
            float dy = b.y - s.pos_y;
            float d2 = dx*dx + dy*dy;
            if (d2 < best_d2) {
                best_d2 = d2;
                best_idx = i;
                best_completed = true;
            }
        }
        if (best_idx >= 0) {
            const dp::world::ConstructionSite* st_ptr = nullptr;
            const dp::world::Building*         bd_ptr = nullptr;
            // The two index spaces overlap; we look up the better one.
            float min_d2 = std::numeric_limits<float>::infinity();
            float bx = 0.0f, by = 0.0f;
            for (const auto& site : buildings.sites()) {
                float dx = site.x - s.pos_x, dy = site.y - s.pos_y;
                float d2 = dx*dx + dy*dy;
                if (d2 < min_d2) { min_d2 = d2; bx = site.x; by = site.y; best_completed = site.completed; (void)st_ptr; }
            }
            for (const auto& bld : buildings.buildings()) {
                float dx = bld.x - s.pos_x, dy = bld.y - s.pos_y;
                float d2 = dx*dx + dy*dy;
                if (d2 < min_d2) { min_d2 = d2; bx = bld.x; by = bld.y; best_completed = true; (void)bd_ptr; }
            }
            out[p++] = clamp_norm((bx - s.pos_x) / VISION_RANGE);
            out[p++] = clamp_norm((by - s.pos_y) / VISION_RANGE);
            out[p++] = best_completed ? 1.0f : 0.0f;
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

    // p should equal SIZE here.
}

}  // namespace dp::agent
