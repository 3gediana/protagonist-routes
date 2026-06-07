#pragma once

#include "core/interfaces/IPerception.h"

namespace neuro::routes::protagonist {

// Phase D (one-pot unified bloodline): kinship recognition channel.
//
// REAL implementation (2026-06): reads the co-present protagonists, finds the
// nearest one, and reports genetic relatedness to it plus the direction toward
// it. Relatedness is the cosine similarity of the two brains' genetic
// fingerprints (a deterministic low-dim projection of the input->hidden weight
// vector). Because reproduction blends/mutates parent weight vectors, offspring
// sit near parents in weight space, so fingerprint similarity tracks kinship
// (same principle as NEAT compatibility distance).
//
// kDim stays 3 so the unified input_dim is unchanged: switching this channel on
// only changes the VALUES in these slots (previously zeros). The one-pot
// surgery already zeroed the corresponding input->hidden weights, so the first
// step after activation is unchanged (weight * signal = 0) and evolution grows
// the weights from zero.
//   [0] nearest-kin relatedness (0..1)
//   [1..2] direction to nearest kin (unit dx, dy)
class KinPerception final : public IPerception {
public:
    static constexpr std::size_t kDim = 3;

    explicit KinPerception(float sense_radius) : sense_radius_(sense_radius) {}

    std::size_t dim() const override { return kDim; }
    void sense(const IAgent& self, const IWorld& world, std::span<float> out) const override;
    const char* name() const override { return "kin"; }

private:
    float sense_radius_;
};

}  // namespace neuro::routes::protagonist
