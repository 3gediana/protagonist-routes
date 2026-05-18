#pragma once

#include "core/interfaces/IPerception.h"
#include "core/types/Aliases.h"

namespace neuro {

class SelfPerception final : public IPerception {
public:
    SelfPerception(float max_energy, Tick max_age_ticks);

    std::size_t dim() const override;
    void sense(const IAgent& self, const IWorld& world, std::span<float> out) const override;
    const char* name() const override;

private:
    float max_energy_;
    Tick max_age_ticks_;
};

}  // namespace neuro
