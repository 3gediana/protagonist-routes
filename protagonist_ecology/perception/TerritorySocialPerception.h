#pragma once

#include "core/interfaces/IPerception.h"

namespace neuro::routes::protagonist {

// Phase E (one-pot unified bloodline): higher-order social structure channel
// (territory / dominance rank / alliance).
//
// REAL implementation (2026-06): all four dims are computed from data that
// already exists in the running sim, so switching this channel on changes only
// the VALUES in these slots (previously zeros). The one-pot surgery already
// zeroed the matching input->hidden weights, so the first step after activation
// is unchanged and evolution grows the weights from zero.
//   [0] inside own (colony) territory (0/1)  -- BaseLayer::inBase(self)
//   [1] local dominance rank (0..1)          -- energy percentile vs nearby kin
//   [2] nearest-ally relatedness/affinity     -- fingerprint cosine to nearest peer
//   [3] nearest-rival threat (0..1)           -- strength*proximity of nearest non-kin/predator
class TerritorySocialPerception final : public IPerception {
public:
    static constexpr std::size_t kDim = 4;

    explicit TerritorySocialPerception(float sense_radius) : sense_radius_(sense_radius) {}

    std::size_t dim() const override { return kDim; }
    void sense(const IAgent& self, const IWorld& world, std::span<float> out) const override;
    const char* name() const override { return "territory_social"; }

private:
    float sense_radius_;
};

}  // namespace neuro::routes::protagonist
