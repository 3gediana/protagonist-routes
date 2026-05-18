#pragma once

#include "core/interfaces/IWorldLayer.h"
#include "core/types/Vec2.h"

#include <cstddef>
#include <random>
#include <vector>

namespace neuro {

class HeavyResourceLayer final : public IWorldLayer {
public:
    static constexpr const char* kName = "heavy_resource";

    struct HeavyResource {
        Vec2 position;
        float amount;
        std::size_t required_workers;
        std::size_t current_workers;
        std::size_t ticks_with_workers;
        std::size_t required_ticks;
        bool available;
    };

    HeavyResourceLayer(std::size_t count,
                       float amount,
                       std::size_t required_workers,
                       std::size_t required_ticks,
                       float collect_radius,
                       std::uint32_t seed);

    const char* layerName() const override { return kName; }
    void onAttach(IWorld& world) override;
    void tick(IWorld& world, float dt) override;

    const std::vector<HeavyResource>& resources() const { return resources_; }

private:
    std::size_t count_;
    float amount_;
    std::size_t required_workers_;
    std::size_t required_ticks_;
    float collect_radius_;
    std::mt19937 rng_;
    Vec2 world_size_;
    std::vector<HeavyResource> resources_;
};

}  // namespace neuro
