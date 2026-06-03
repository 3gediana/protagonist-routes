#include "world/FarmingSystem.hpp"

#include "world/BuildingSystem.hpp"

#include <algorithm>

namespace dp::world {

int FarmingSystem::plant_seed(float x, float y, float z, PlantKind kind) {
    FarmPlot p;
    p.x = x; p.y = y; p.z = z;
    p.seed_kind = kind;
    p.growth = 0.0f;
    p.water_level = 0.5f;  // small starter splash
    p.ripe = false;
    p.harvested = false;
    plots_.push_back(p);
    return int(plots_.size()) - 1;
}

bool FarmingSystem::water_at(float x, float y, float reach) {
    float best_d2 = reach * reach;
    int   best_idx = -1;
    for (int i = 0; i < int(plots_.size()); ++i) {
        if (plots_[i].harvested) continue;
        float dx = plots_[i].x - x;
        float dy = plots_[i].y - y;
        float d2 = dx*dx + dy*dy;
        if (d2 < best_d2) {
            best_d2 = d2;
            best_idx = i;
        }
    }
    if (best_idx < 0) return false;
    plots_[best_idx].water_level = std::min(1.0f,
        plots_[best_idx].water_level + water_step);
    return true;
}

void FarmingSystem::rain(float dt, float per_s) {
    if (per_s <= 0.0f) return;
    float add = per_s * dt;
    for (auto& p : plots_) {
        if (p.harvested) continue;
        p.water_level = std::min(1.0f, p.water_level + add);
    }
}

std::vector<PlantKind> FarmingSystem::harvest_at(float x, float y, float reach) {
    std::vector<PlantKind> out;
    float r2 = reach * reach;
    for (auto& p : plots_) {
        if (p.harvested || !p.ripe) continue;
        float dx = p.x - x, dy = p.y - y;
        if (dx*dx + dy*dy <= r2) {
            p.harvested = true;
            out.push_back(p.seed_kind);
        }
    }
    return out;
}

void FarmingSystem::tick(float dt, const BuildingSystem* buildings) {
    // Remove plots that were harvested (we don't keep the dirt around).
    plots_.erase(std::remove_if(plots_.begin(), plots_.end(),
        [](const FarmPlot& p) { return p.harvested; }), plots_.end());

    for (auto& p : plots_) {
        if (p.ripe) continue;
        p.water_level -= water_drain_per_second * dt;
        if (p.water_level < 0.0f) p.water_level = 0.0f;
        // Only grow while there's water.
        if (p.water_level > 0.05f) {
            // D-091: plots that sit inside any completed building's coverage
            // grow building_growth_mult times faster. That's the blueprint's
            // 房子周围庄稼长得快 — settlement should reward 围家种田.
            float mult = 1.0f;
            if (buildings != nullptr &&
                buildings->covers(p.x, p.y)) {
                mult = building_growth_mult;
            }
            p.growth += growth_per_second * dt * mult;
            if (p.growth >= 1.0f) {
                p.growth = 1.0f;
                p.ripe = true;
            }
        }
    }
}

int FarmingSystem::ripe_count() const {
    int n = 0;
    for (const auto& p : plots_) if (p.ripe && !p.harvested) ++n;
    return n;
}

void FarmingSystem::clear() {
    plots_.clear();
}

}  // namespace dp::world
