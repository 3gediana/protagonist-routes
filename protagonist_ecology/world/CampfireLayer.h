#pragma once

#include "core/interfaces/IWorldLayer.h"
#include "core/types/Vec2.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <vector>

namespace neuro {
class AgentLayer;
class IWorld;
}

namespace neuro::routes::protagonist {

class BaseLayer;

struct CampfireSite {
    std::size_t id;
    Vec2 position;
    float intensity;
    bool active;
    float fuel = 0.0f;   // remaining burn time in seconds; active fire burns
                         // this down and goes dormant when it hits 0.
};

class CampfireLayer final : public IWorldLayer {
public:
    static constexpr const char* kName = "protagonist_campfire";

    CampfireLayer(std::size_t fire_count,
                  float spawn_radius,
                  float warmth_radius,
                  float danger_radius,
                  float warmth_energy_bonus,
                  float danger_damage,
                  std::uint32_t seed);

    const char* layerName() const override { return kName; }
    void onAttach(IWorld& world) override;
    void tick(IWorld& world, float dt) override;
    int tickOrder() const override { return 150; }

    const std::vector<CampfireSite>& fires() const { return fires_; }
    std::optional<CampfireSite> nearestFire(Vec2 position) const;
    bool inWarmthZone(Vec2 position) const;
    bool inDangerZone(Vec2 position) const;
    bool igniteNextDormant();
    // Agent-driven ignition: light (or refuel) the nearest fire site within
    // `radius` of `position`, adding `fuel_add` seconds of burn (capped at
    // fuel_max). Returns true if a fire was lit or refuelled.
    bool igniteOrRefuelNear(Vec2 position, float radius, float fuel_add);
    std::size_t activeFireCount() const;
    float warmthRadius() const { return warmth_radius_; }
    float dangerRadius() const { return danger_radius_; }

private:
    void seedFires(Vec2 world_size);

    BaseLayer* base_ = nullptr;
    AgentLayer* agents_ = nullptr;
    std::size_t fire_count_;
    float spawn_radius_;
    float warmth_radius_;
    float danger_radius_;
    float warmth_energy_bonus_;
    float danger_damage_;
    std::mt19937 rng_;
    std::vector<CampfireSite> fires_;
};

}  // namespace neuro::routes::protagonist
