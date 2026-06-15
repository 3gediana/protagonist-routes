#pragma once

#include "core/interfaces/IPerception.h"

namespace neuro::routes::protagonist {

// Phase F (one-pot unified bloodline): persistent world-history channel (what
// the agent's lineage has durably changed about the world).
//
// REAL implementation (2026-06): both dims read existing sim state, so turning
// this channel on changes only the VALUES in these slots (previously zeros);
// the one-pot zeroed weights keep the first post-activation step unchanged.
//   [0] local accumulated-development level (0..1)
//       -- density of durable structures (campfires + worksites, +base) nearby
//   [1] world age signal (0..1) -- saturating map of world clock time
class WorldHistoryPerception final : public IPerception {
public:
    static constexpr std::size_t kDim = 2;

    explicit WorldHistoryPerception(float sense_radius) : sense_radius_(sense_radius) {}

    std::size_t dim() const override { return kDim; }
    void sense(const IAgent& self, const IWorld& world, std::span<float> out) const override;
    const char* name() const override { return "world_history"; }

private:
    float sense_radius_;
};

}  // namespace neuro::routes::protagonist
