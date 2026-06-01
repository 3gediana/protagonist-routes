#include "perception/MemoryTrialPerception.h"

#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "world/MemoryTrialLayer.h"

#include <algorithm>

namespace neuro::routes::protagonist {

void MemoryTrialPerception::sense(const IAgent& agent, const IWorld& world, std::span<float> output) const {
    if (output.size() < kDim) {
        return;
    }

    std::fill(output.begin(), output.end(), 0.0f);

    const auto* memory_trial = world.getLayer<MemoryTrialLayer>();
    if (memory_trial == nullptr) {
        return;
    }

    const auto cue = memory_trial->cueFor(agent.position());
    output[0] = cue[0];
    output[1] = cue[1];
}

}  // namespace neuro::routes::protagonist
