#pragma once

// L2.1 v2 (SPECIES_DIFFERENTIATION_SPEC.md § 1.2 small_forager row).
// Each tick, queries TerrainLayer for whether the owning agent's
// current position is inside a cave and writes the result back to
// the agent's is_in_cave_ flag. AgentSocialPerception then filters
// caved-prey out for observers outside the cave, completing the
// hide_in_terrain mechanic.
//
// Passive: consumedOutputs() == 0. Attached to background SmallForager.
// Cheap: O(1) per tick (single noise-field sample inside TerrainLayer).
// No-op when the world has no TerrainLayer.

#include "core/interfaces/IAgentCapability.h"

namespace neuro::routes::protagonist {

class CaveHideCapability final : public IAgentCapability {
public:
    CaveHideCapability() = default;

    std::size_t consumedOutputs() const override { return 0; }
    void apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) override;
    const char* name() const override { return "cave_hide_tracker"; }
};

}  // namespace neuro::routes::protagonist
