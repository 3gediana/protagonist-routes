#pragma once

#include "core/interfaces/IWorldLayer.h"
#include "core/types/Vec2.h"

#include <cstdint>
#include <random>

namespace neuro {

class DiseaseLayer final : public IWorldLayer {
public:
    static constexpr const char* kName = "disease";

    DiseaseLayer(float infection_radius,
                 float infection_prob,
                 float infection_duration,
                 float death_prob,
                 float initial_infection_rate,
                 std::uint32_t seed = 0);

    const char* layerName() const override { return kName; }
    void onAttach(IWorld& world) override;
    void tick(IWorld& world, float dt) override;

    float infectionRadius() const { return infection_radius_; }
    std::size_t infectionDeathCount() const { return infection_deaths_; }

private:
    float infection_radius_;
    float infection_prob_;
    float infection_duration_;
    float death_prob_;
    float initial_infection_rate_;
    std::mt19937 rng_;
    std::size_t infection_deaths_ = 0;
};

}  // namespace neuro
