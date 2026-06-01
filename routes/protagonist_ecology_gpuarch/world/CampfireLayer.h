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
