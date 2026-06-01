#pragma once

#include "core/interfaces/IPerception.h"

namespace neuro::routes::protagonist {

// Phase E (one-pot unified bloodline): higher-order social structure channel
// (territory / dominance rank / alliance). RESERVED placeholder — dims are part
// of the locked unified input encoding so Phase E logic can be switched on later
// WITHOUT changing input_dim. sense() writes zeros until the real logic lands.
//   [0] inside own territory (0/1)
//   [1] local dominance rank (0..1)
//   [2] nearest-ally relatedness/affinity (0..1)
//   [3] nearest-rival threat (0..1)
class TerritorySocialPerception final : public IPerception {
public:
    static constexpr std::size_t kDim = 4;

    std::size_t dim() const override { return kDim; }
    void sense(const IAgent& self, const IWorld& world, std::span<float> out) const override;
    const char* name() const override { return "territory_social"; }
};

}  // namespace neuro::routes::protagonist
