#pragma once

#include "core/interfaces/IWorldLayer.h"
#include "core/types/Aliases.h"
#include "core/types/Vec2.h"

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

struct Worksite {
    std::size_t id;
    Vec2 position;
    float radius;
    bool active;
    bool completed;
    std::size_t stored_sticks;
    std::size_t stored_stones;
    std::size_t required_sticks;
    std::size_t required_stones;
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

private:
    void seedSites(Vec2 world_size);
    void absorbCarriedMaterials(IWorld& world);
    void absorbNearbyMaterials();
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
};

}  // namespace neuro::routes::protagonist
