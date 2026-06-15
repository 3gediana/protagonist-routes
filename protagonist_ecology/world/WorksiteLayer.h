#pragma once

#include "core/interfaces/IWorldLayer.h"
#include "core/types/Aliases.h"
#include "core/types/Vec2.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace neuro {
class IWorld;
class AgentLayer;
class ObjectLayer;
}

namespace neuro::routes::protagonist {

class BaseLayer;

// Layer 5 (FINAL_BLUEPRINT.md §5): building typology. A worksite/building
// has one of these types; each affords different gameplay buffs:
//   Wall      — predator-blocking circle (existing v9 behaviour, default)
//   Shelter   — yurt-like enclosure: rain protection (no hydration recovery,
//               no thunderstorm fires inside; visual = Mongolian-yurt-level)
//   Storage   — material stockpile: nearby agents auto-deposit carried sticks
//   Lookout   — perception bonus: agents inside see further (PerceptionLayer
//               can read worksite-of-type-Lookout to widen sight radius)
enum class BuildingType : std::uint8_t {
    Wall    = 0,
    Shelter = 1,
    Storage = 2,
    Lookout = 3,
};

const char* buildingTypeName(BuildingType t);

struct Worksite {
    std::size_t id;
    Vec2 position;
    float radius;
    bool active;
    bool completed;
    BuildingType building_type{BuildingType::Wall};  // L5
    std::size_t stored_sticks;
    std::size_t stored_stones;
    std::size_t required_sticks;
    std::size_t required_stones;
    // L5 R2: Storage building's overflow stockpile (after construction
    // completes, agents can keep depositing into Storage; these are
    // 'second-tier' resources protected from environmental loss).
    std::size_t stored_sticks_overflow{0};
    std::size_t stored_stones_overflow{0};
    float build_progress;

    // Continuous-time bookkeeping (CT-2/CT-3).
    // build_started_at_seconds: timestamp of the first tick where any agent
    //   contributed to this site's build progress. < 0 means: not started yet.
    // per_agent_contribution_seconds: time-weighted contribution of each agent
    //   that stood inside the site while build_progress advanced. Used by the
    //   credit-assignment ledger (P3) and by the BuildCompleted contributor list.
    // active_builders: agents currently inside the site (used to detect
    //   per-agent BuildStarted on first entry).
    double build_started_at_seconds = -1.0;
    std::unordered_map<AgentId, float> per_agent_contribution_seconds;
    std::unordered_set<AgentId> active_builders;
};

class WorksiteLayer final : public IWorldLayer {
public:
    static constexpr const char* kName = "protagonist_worksite";

    WorksiteLayer(std::size_t site_count,
                  float spawn_radius,
                  float interaction_radius,
                  std::size_t required_sticks,
                  std::size_t required_stones,
                  float assembly_rate,
                  float shelter_energy_bonus,
                  std::uint32_t seed);

    // Layer 5 R2: post-construction setter for building type weights.
    // Factory calls this before onAttach if it wants non-default
    // (all-Wall) distribution. Weights are normalized internally.
    void setBuildingTypeWeights(float wall, float shelter, float storage, float lookout);

    const char* layerName() const override { return kName; }
    void onAttach(IWorld& world) override;
    void tick(IWorld& world, float dt) override;
    int tickOrder() const override { return 155; }

    const std::vector<Worksite>& sites() const { return sites_; }
    std::optional<Worksite> nearestActiveSite(Vec2 position) const;
    bool inActiveSite(Vec2 position) const;
    bool activateNextDormant();
    std::size_t activeSiteCount() const;
    std::size_t completedSiteCount() const;

    // Emergent shelter L1: a completed worksite represents a stack of
    // sticks/stones pushed up against the nest perimeter and forms a
    // collidable wall. Predators are pushed back to the boundary if they
    // try to walk through; protagonists and other agents are unaffected,
    // so they can still enter the nest / shelter / pick up the stockpile.
    // Returns true iff `pos` is inside any completed site.
    bool blocksPredatorAt(Vec2 pos) const;
    // Spawns a worksite that is already 100% completed at `position`. Used
    // by tasks that need a pre-built wall in the eval (e.g. before the
    // protagonists themselves can build).
    void spawnCompletedAt(Vec2 position);

    // Layer 5: returns the highest-priority building of `type` whose radius
    // contains `pos`, or nullopt. Used by other layers to query buffs.
    std::optional<Worksite> buildingAt(Vec2 pos, BuildingType type) const;
    // Returns true iff `pos` is sheltered from rain (i.e. inside a completed
    // Shelter-type building). WeatherLayer reads this to skip hydration
    // recovery / thunderstorm ignition for sheltered agents.
    bool isShelteredAt(Vec2 pos) const;

    // Layer 5 R2: total counts across all completed Storage buildings.
    // Useful for protagonist stockpile-aware perception / reward logic.
    std::size_t totalStorageSticks() const;
    std::size_t totalStorageStones() const;

private:
    void seedSites(Vec2 world_size);
    void absorbCarriedMaterials(IWorld& world);
    void absorbNearbyMaterials();
    // L5 R2: completed Storage buildings keep absorbing carried sticks/stones
    // into their overflow stockpile. Protected against tree-fall etc.
    void absorbToCompletedStorages(IWorld& world);
    void progressConstruction(IWorld& world, float dt);
    // Pushes predators that walked into a completed wall back out to its
    // boundary. Called from tick() at tickOrder=155 (after AgentLayer
    // tick=100). See FINAL_STATE_VISION § 3.7 (emergent shelter L1).
    void enforcePredatorObstacles();

    BaseLayer* base_ = nullptr;
    AgentLayer* agents_ = nullptr;
    ObjectLayer* objects_ = nullptr;
    std::size_t site_count_;
    float spawn_radius_;
    float interaction_radius_;
    std::size_t required_sticks_;
    std::size_t required_stones_;
    float assembly_rate_;
    float shelter_energy_bonus_;
    std::mt19937 rng_;
    std::vector<Worksite> sites_;
    // L5 R2: building type distribution. Normalized so it sums to 1.
    std::array<float, 4> building_type_weights_{1.0f, 0.0f, 0.0f, 0.0f};
};

}  // namespace neuro::routes::protagonist
