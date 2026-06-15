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
// D-058: Shed cost cut from (4 wood + 2 grass = 6 deposits) down to
// (1 wood + 1 grass = 2 deposits). D-056/D-057 trial showed PPO could
// not reliably finish a 6-deposit chain - the +10 prize at the end was
// too far from the "I should commit to building" decision under γ=0.99.
// Shrinking the chain to 2 deposits lets PPO actually see the
// completion reward signal during training. Blueprint line 30 says
// "反复搬木头石头草放上去" - which is still true at 2 deposits, just
// the smallest version. WoodHouse/StoneHouse/BigHouse keep their
// original costs so progression has a difficulty ladder.
// D-091: Shed coverage_radius bumped 3.5 -> 5.0. The 3.5 m porch was
// physically smaller than the Shed itself.
// D-095: bumped again 5.0 -> 15.0 (Shed), 6 -> 18 (Wood), 7 -> 22 (Stone),
// 8.5 -> 28 (Big). D-094's 1175k+1640k step partials both showed PPO
// builds the spawn Shed early (bldg_ticks 229-406 / first 100 ep) and
// then *unlearns* sheltering (bldg_ticks 5-43 / last 100 ep). Diagnosis:
// the 5m circle is so small the agent crosses it in 2-3 ticks during any
// foraging loop, so the only way to stay sheltered is to literally camp
// on top of the building - which conflicts with eating/drinking/farming.
// A 15m "home zone" lets the agent forage within ~10m of the house and
// still register sheltered, which matches the blueprint's 村落
// (settlement / courtyard) idea where the area around the house is
// implicitly part of the home, not just the four walls. 28m for BigHouse
// gives a proper village courtyard. Costs / Mid gate / foot_radius are
// untouched, this is purely a comfort-zone expansion.
const std::array<BuildingRecipe, 4> kRecipes = {{
    { BuildingType::Shed,        "shed",        1, 0, 1, dp::agent::ProgressStage::Mid,  1.2f, 15.0f },
    { BuildingType::WoodHouse,   "wood_house",  8, 2, 4, dp::agent::ProgressStage::Mid,  1.8f, 18.0f },
    { BuildingType::StoneHouse,  "stone_house", 6, 8, 2, dp::agent::ProgressStage::Mid,  2.0f, 22.0f },
    { BuildingType::BigHouse,    "big_house",  12,10, 6, dp::agent::ProgressStage::Mid,  2.6f, 28.0f },
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

int BuildingSystem::admin_place_site(BuildingType type, float x, float y, float z) {
    // D-086: env-side seeding (Environment::reset spawns one Shed per
    // episode 8m N of agent). Bypasses stage gate + single-site rule so
    // the target is always there from step 0 -- shortens the build-chain
    // credit assignment that defeated 5 prior 5M runs (~7 houses total).
    ConstructionSite s;
    s.type = type;
    s.x = x; s.y = y; s.z = z;
    sites_.push_back(s);
    return int(sites_.size()) - 1;
}

int BuildingSystem::place_site(BuildingType type, float x, float y, float z,
                               const dp::agent::ProgressTracker& progress) {
    const auto& r = building_recipe(type);
    // Stage gate
    if (static_cast<int>(progress.stage()) < static_cast<int>(r.min_stage)) {
        return -1;
    }
    // D-055: blueprint line 30 says "放图纸" (singular). D-053 PPO was
    // mashing place_blueprint at 14.3% of all triggered ticks (981 sites
    // across 33 ep) because it was a free no-op once Mid was unlocked.
    // Refusing a second incomplete site forces the policy to actually
    // commit to one project at a time - which is what the blueprint
    // means by "in the same spot, deposit until it's done".
    for (const auto& existing : sites_) {
        if (!existing.completed) return -1;
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
