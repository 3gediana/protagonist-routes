#pragma once

#include "core/interfaces/IPerception.h"
#include "core/types/Aliases.h"

namespace neuro {

class SelfPerception final : public IPerception {
public:
    SelfPerception(float max_energy, SimTimeSeconds max_age_seconds);

    std::size_t dim() const override;
    void sense(const IAgent& self, const IWorld& world, std::span<float> out) const override;
    const char* name() const override;

private:
    float max_energy_;
    SimTimeSeconds max_age_seconds_;
};

}  // namespace neuro
