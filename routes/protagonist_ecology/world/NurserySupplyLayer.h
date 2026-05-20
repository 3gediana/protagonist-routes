#pragma once

#include "core/interfaces/IWorldLayer.h"

#include <cstddef>
#include <cstdint>
#include <random>

namespace neuro {
class IWorld;
class ObjectLayer;
}

namespace neuro::routes::protagonist {

class BaseLayer;

class NurserySupplyLayer final : public IWorldLayer {
public:
    static constexpr const char* kName = "protagonist_nursery_supply";

    NurserySupplyLayer(std::size_t min_nearby_sticks,
                       std::size_t replenish_batch,
                       float near_radius,
                       float spawn_radius,
                       std::size_t cooldown_ticks,
                       std::uint32_t seed);

    const char* layerName() const override { return kName; }
    void onAttach(IWorld& world) override;
    void tick(IWorld& world, float dt) override;
    int tickOrder() const override { return 138; }

private:
    bool hasLooseStone() const;
    std::size_t nearbyLooseMaterials() const;
    void replenish(IWorld& world);

    BaseLayer* base_ = nullptr;
    ObjectLayer* objects_ = nullptr;
    std::size_t min_nearby_sticks_;
    std::size_t replenish_batch_;
    float near_radius_;
    float spawn_radius_;
    std::size_t cooldown_ticks_;
    std::uint64_t last_replenish_tick_ = 0;
    std::mt19937 rng_;
};

}  // namespace neuro::routes::protagonist
