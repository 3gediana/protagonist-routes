#include "world/BuildingSystem.hpp"

#include <array>
#include <cmath>

namespace dp::world {

const char* building_type_name(BuildingType t) {
    switch (t) {
        case BuildingType::Shed:       return "shed";
        case BuildingType::WoodHouse:  return "wood_house";
        case BuildingType::StoneHouse: return "stone_house";
        case BuildingType::BigHouse:   return "big_house";
    }
    return "?";
}

namespace {
// Recipe table. Costs scale with tier; min_stage gates progression.
const std::array<BuildingRecipe, 4> kRecipes = {{
    { BuildingType::Shed,        "shed",        4, 0, 2, dp::agent::ProgressStage::Mid,  1.2f, 3.5f },
    { BuildingType::WoodHouse,   "wood_house",  8, 2, 4, dp::agent::ProgressStage::Mid,  1.8f, 4.5f },
    { BuildingType::StoneHouse,  "stone_house", 6, 8, 2, dp::agent::ProgressStage::Mid,  2.0f, 5.5f },
    { BuildingType::BigHouse,    "big_house",  12,10, 6, dp::agent::ProgressStage::Mid,  2.6f, 7.0f },
}};
}  // namespace

const BuildingRecipe& building_recipe(BuildingType t) {
    int idx = static_cast<int>(t);
    if (idx < 0 || idx >= int(kRecipes.size())) idx = 0;
    return kRecipes[idx];
}

bool ConstructionSite::ready() const {
    const auto& r = building_recipe(type);
    return wood_deposited  >= r.wood_required
        && stone_deposited >= r.stone_required
        && grass_deposited >= r.grass_required;
}

int ConstructionSite::remaining(dp::agent::ItemKind k) const {
    const auto& r = building_recipe(type);
    int need = 0, have = 0;
    switch (k) {
        case dp::agent::ItemKind::Wood:  need = r.wood_required;  have = wood_deposited;  break;
        case dp::agent::ItemKind::Stone: need = r.stone_required; have = stone_deposited; break;
        case dp::agent::ItemKind::Grass: need = r.grass_required; have = grass_deposited; break;
        default: return 0;  // crafted items not accepted
    }
    int rem = need - have;
    return rem > 0 ? rem : 0;
}

bool ConstructionSite::deposit(dp::agent::ItemKind k) {
    if (completed) return false;
    if (remaining(k) <= 0) return false;
    switch (k) {
        case dp::agent::ItemKind::Wood:  ++wood_deposited;  break;
        case dp::agent::ItemKind::Stone: ++stone_deposited; break;
        case dp::agent::ItemKind::Grass: ++grass_deposited; break;
        default: return false;
    }
    return true;
}

float ConstructionSite::material_progress() const {
    const auto& r = building_recipe(type);
    int need = r.wood_required + r.stone_required + r.grass_required;
    if (need <= 0) return 1.0f;
    int have = wood_deposited + stone_deposited + grass_deposited;
    float p = static_cast<float>(have) / static_cast<float>(need);
    if (p < 0.0f) p = 0.0f;
    if (p > 1.0f) p = 1.0f;
    return p;
}

int BuildingSystem::place_site(BuildingType type, float x, float y, float z,
                               const dp::agent::ProgressTracker& progress) {
    const auto& r = building_recipe(type);
    // Stage gate
    if (static_cast<int>(progress.stage()) < static_cast<int>(r.min_stage)) {
        return -1;
    }
    ConstructionSite s;
    s.type = type;
    s.x = x; s.y = y; s.z = z;
    sites_.push_back(s);
    return int(sites_.size()) - 1;
}

int BuildingSystem::try_deposit(float ax, float ay, float az, float reach,
                                dp::agent::ItemKind kind) {
    float best_d2  = reach * reach;
    int   best_idx = -1;
    for (int i = 0; i < int(sites_.size()); ++i) {
        if (sites_[i].completed) continue;
        if (sites_[i].remaining(kind) <= 0) continue;
        float dx = sites_[i].x - ax;
        float dy = sites_[i].y - ay;
        float d2 = dx*dx + dy*dy;
        if (d2 < best_d2) {
            best_d2 = d2;
            best_idx = i;
        }
    }
    if (best_idx < 0) return -1;
    sites_[best_idx].deposit(kind);
    return best_idx;
}

int BuildingSystem::tick_finalize() {
    int promoted = 0;
    for (auto& s : sites_) {
        if (s.completed) continue;
        if (s.ready()) {
            s.completed = true;
            const auto& r = building_recipe(s.type);
            Building b;
            b.type = s.type;
            b.x = s.x; b.y = s.y; b.z = s.z;
            b.coverage_radius = r.coverage_radius;
            buildings_.push_back(b);
            ++promoted;
        }
    }
    return promoted;
}

bool BuildingSystem::covers(float px, float py) const {
    for (const auto& b : buildings_) {
        float dx = px - b.x;
        float dy = py - b.y;
        if (dx*dx + dy*dy <= b.coverage_radius * b.coverage_radius) {
            return true;
        }
    }
    return false;
}

void BuildingSystem::clear() {
    sites_.clear();
    buildings_.clear();
}

}  // namespace dp::world
