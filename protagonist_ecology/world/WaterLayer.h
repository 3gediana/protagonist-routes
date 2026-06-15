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
class TerrainLayer;
}

namespace neuro::routes::protagonist {

class BaseLayer;

struct WaterSource {
    std::size_t id;
    Vec2 position;
    float radius;
};

class WaterLayer final : public IWorldLayer {
public:
    static constexpr const char* kName = "protagonist_water";

    WaterLayer(std::size_t source_count,
               float spawn_radius_min,
               float spawn_radius_max,
               float source_radius,
               float drink_radius,
               float drink_rate,
               std::uint32_t seed);

    const char* layerName() const override { return kName; }
    void onAttach(IWorld& world) override;
    void tick(IWorld& world, float dt) override;
    int tickOrder() const override { return 140; }

    const std::vector<WaterSource>& sources() const { return sources_; }
    std::optional<WaterSource> nearestWater(Vec2 position) const;
    bool inDrinkZone(Vec2 position) const;
    float drinkRate() const { return drink_rate_; }
    float drinkRadius() const { return drink_radius_; }

private:
    void seedSources(Vec2 world_size);

    BaseLayer* base_ = nullptr;
    const TerrainLayer* terrain_ = nullptr;
    std::size_t source_count_;
    float spawn_radius_min_;
    float spawn_radius_max_;
    float source_radius_;
    float drink_radius_;
    float drink_rate_;
    std::mt19937 rng_;
    std::vector<WaterSource> sources_;
};

}  // namespace neuro::routes::protagonist
