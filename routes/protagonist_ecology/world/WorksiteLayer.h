#pragma once

#include "core/interfaces/IWorldLayer.h"
#include "core/types/Vec2.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
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

private:
    void seedSites(Vec2 world_size);
    void absorbCarriedMaterials();
    void absorbNearbyMaterials();
    void progressConstruction(IWorld& world, float dt);

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
