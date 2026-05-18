#include "core/perception/TimeOfDayPerception.h"

#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/TimeOfDayLayer.h"

namespace neuro {

void TimeOfDayPerception::sense(const IAgent& agent, const IWorld& world, std::span<float> output) const {
    (void)agent;
    if (output.size() < kDim) return;

    const auto* tod = world.getLayer<TimeOfDayLayer>();
    if (tod == nullptr) {
        output[0] = 0.5f;
        output[1] = 0.5f;
        return;
    }

    output[0] = tod->timeOfDay();
    output[1] = tod->lightLevel();
}

}  // namespace neuro
