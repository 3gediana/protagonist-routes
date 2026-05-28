#pragma once

#include "agent/Inventory.hpp"
#include "agent/ProgressTracker.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace dp::world {

// Four house tiers the agent can construct from collected resources.
// Each one unlocks more shelter benefits and storage capacity.
// D-042 plans: add Pen (animal pen) + Storage (chest) - see docs/D-042_PHASE2_DESIGN.md.
enum class BuildingType : uint8_t {
    Shed       = 0,  // smallest, fastest to build (mid-stage starter)
    WoodHouse  = 1,  // mid-stage: timber-only walls
    StoneHouse = 2,  // mid-late: stone walls, better shelter
    BigHouse   = 3,  // late: large, storage + sleep + temp recovery
};

const char* building_type_name(BuildingType t);

struct BuildingRecipe {
    BuildingType        type;
    const char*         name;
    int                 wood_required;
    int                 stone_required;
    int                 grass_required;
    dp::agent::ProgressStage min_stage;
    float               foot_radius;        // size of build footprint in metres
    float               coverage_radius;    // shelter coverage radius once built
};

// Look up the canonical recipe for a building type.
const BuildingRecipe& building_recipe(BuildingType t);

// A "blueprint placed on the ground" - agent walks materials up to it,
// and when the recipe is satisfied we promote it to a Building (3D model).
struct ConstructionSite {
    BuildingType type;
    float        x, y, z;
    int          wood_deposited  = 0;
    int          stone_deposited = 0;
    int          grass_deposited = 0;
    bool         completed       = false;

    bool ready() const;
    int  remaining(dp::agent::ItemKind k) const;
    // Returns true if the deposit was accepted (i.e. the site still needs
    // this kind). Wrong kinds are politely rejected so the caller can
    // refund the inventory slot.
    bool deposit(dp::agent::ItemKind k);

    // Material progress in [0, 1] across all three resource kinds.
    // Used by WorldRenderer to grow the half-built geometry stage by
    // stage (foundation -> walls -> roof) instead of materialising the
    // whole house in one frame.
    float material_progress() const;
};

// A finished house: 3D anchor with type-specific size, shelter coverage,
// and (planned) storage capacity.
struct Building {
    BuildingType type;
    float        x, y, z;
    float        coverage_radius;
};

class BuildingSystem {
public:
    // Place a brand new construction site at (x,y,z). Returns the index
    // of the new site (>=0) or -1 if blocked (e.g. progression locked).
    int place_site(BuildingType type, float x, float y, float z,
                   const dp::agent::ProgressTracker& progress);

    // D-086: Admin-placed site bypasses stage gating + single-site rule.
    // Used by Environment::reset to seed an always-available Shed target
    // near spawn so PPO has a concrete deposit target from step 0 instead
    // of a 200+ step credit-assignment chain. NOT exposed to the agent.
    int admin_place_site(BuildingType type, float x, float y, float z);

    // Drop one unit of `kind` from the agent's inventory into the closest
    // site within `reach` metres. Returns the site index hit, or -1 if
    // none accepted the deposit (no sites in range, or all sites full).
    int try_deposit(float ax, float ay, float az, float reach,
                    dp::agent::ItemKind kind);

    // Per-tick: promote any site whose deposits meet the recipe to a
    // completed Building. Returns the number of sites promoted this tick.
    int tick_finalize();

    // True if (px, py) is inside the shelter coverage of any completed
    // building. Used by VitalSystem / AnimalSystem in place of the early
    // Shelter coverage check.
    bool covers(float px, float py) const;

    const std::vector<ConstructionSite>& sites()     const { return sites_; }
    const std::vector<Building>&         buildings() const { return buildings_; }

    void clear();  // episode reset

private:
    std::vector<ConstructionSite> sites_;
    std::vector<Building>         buildings_;
};

}  // namespace dp::world
