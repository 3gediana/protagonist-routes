#pragma once

#include "core/interfaces/IAgentCapability.h"

namespace neuro::routes::protagonist {

// Layer 5 #6: lets the protagonist softmax-vote which BuildingType
// (Wall / Shelter / Storage / Lookout) the next-to-be-activated
// dormant worksite should become. Consumes 4 action outputs; the
// argmax over them sets `Agent::buildingTypeChoice()` (0..3).
//
// WorksiteLayer::activateNextDormant() reads nearby protagonists'
// votes and majority-overrides the seeded type distribution. If
// the feature is disabled (toml flag off), this capability is not
// attached at all and the action_dim stays at the legacy value, so
// existing checkpoints keep loading.
class ChooseBuildingTypeCapability final : public IAgentCapability {
public:
    ChooseBuildingTypeCapability() = default;

    std::size_t consumedOutputs() const override { return 4; }
    void apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) override;
    const char* name() const override { return "choose_building_type"; }
};

}  // namespace neuro::routes::protagonist
