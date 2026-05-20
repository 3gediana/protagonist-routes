#include "perception/MemoryCognitionPerception.h"

#include "core/agent/Agent.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "brain/ProtagonistBrain.h"

#include <algorithm>

namespace neuro::routes::protagonist {

MemoryCognitionPerception::MemoryCognitionPerception() {}

void MemoryCognitionPerception::sense(const IAgent& agent, const IWorld& world, std::span<float> output) const {
    (void)world;
    if (output.size() < kDim) {
        return;
    }

    const auto* brain = dynamic_cast<const ProtagonistBrain*>(&agent.brain());
    if (brain == nullptr) {
        // Fallback if not protagonist
        std::fill(output.begin(), output.end(), 0.0f);
        return;
    }

    const auto& spatial = brain->spatialMemory();
    const auto& goal = brain->goalMemory();
    const auto pos = agent.position();

    // 1. Spatial queries (dx, dy normalized by world span)
    constexpr float kWorldSpan = 200.0f;

    auto sense_target = [&](Vec2 target, std::size_t out_offset) {
        if (target.x == 0.0f && target.y == 0.0f) {
            output[out_offset] = 0.0f;
            output[out_offset + 1] = 0.0f;
        } else {
            const Vec2 diff = target - pos;
            output[out_offset] = std::clamp(diff.x / kWorldSpan, -1.0f, 1.0f);
            output[out_offset + 1] = std::clamp(diff.y / kWorldSpan, -1.0f, 1.0f);
        }
    };

    sense_target(spatial.findNearestWater(pos, 0.15f), 0);
    sense_target(spatial.findNearestBase(pos, 0.15f), 2);
    sense_target(spatial.findNearestObject(pos, 0.15f), 4);
    sense_target(spatial.findNearestDanger(pos, 0.15f), 6);

    // 2. Goal states
    // Output current goal as a normalized index in [0, 1]
    const float goal_idx = static_cast<float>(goal.currentGoal()) / static_cast<float>(GoalType::Count);
    output[8] = std::clamp(goal_idx, 0.0f, 1.0f);
    output[9] = std::clamp(goal.commitment(), 0.0f, 1.0f);
    output[10] = std::clamp(goal.elapsedSeconds() / 100.0f, 0.0f, 1.0f);
    output[11] = std::clamp(goal.completionProgress(), 0.0f, 1.0f);
}

}  // namespace neuro::routes::protagonist
