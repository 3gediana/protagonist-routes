#pragma once

#include "core/interfaces/IPerception.h"

namespace neuro::routes::protagonist {

// Phase C (one-pot integration): self-heading proprioception. Gives the
// protagonist a CONTINUOUS, circular encoding of its own facing direction:
//   [0] sin(heading_angle)
//   [1] cos(heading_angle)
// where heading = normalize(velocity). When (near) stationary the last
// non-trivial heading is held (mutable per-agent cache), so a resting agent
// still knows which way it faces. Each protagonist owns its own perception
// instance (see createProtagonist), so the cache stays deterministic under
// adt=1 replay.
//
// Rationale: once FOV/occlusion gating zeroes out-of-view targets, the brain
// must know its own orientation to interpret which sectors of the world it can
// currently see vs must recall. Without a heading signal the gated exteroceptive
// inputs are ambiguous.
//
// Enabling adds kDim input dims (fresh lineage; gated by
// protagonist_self_heading_enabled, default false so existing checkpoints stay
// loadable).
class SelfHeadingPerception final : public IPerception {
public:
    static constexpr std::size_t kDim = 2;

    std::size_t dim() const override { return kDim; }
    void sense(const IAgent& self, const IWorld& world, std::span<float> out) const override;
    const char* name() const override { return "self_heading"; }

private:
    mutable float last_sin_ = 0.0f;  // default facing +x => sin=0, cos=1
    mutable float last_cos_ = 1.0f;
    mutable bool  has_prev_ = false;
};

}  // namespace neuro::routes::protagonist