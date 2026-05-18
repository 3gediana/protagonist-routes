#include "core/perception/SeasonPerception.h"

#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/SeasonLayer.h"

namespace neuro {

void SeasonPerception::sense(const IAgent& agent, const IWorld& world, std::span<float> output) const {
    (void)agent;
    if (output.size() < kDim) return;

    const auto* season = world.getLayer<SeasonLayer>();
    if (season == nullptr) {
        output[0] = 0.0f;
        output[1] = 1.0f;
        return;
    }

    output[0] = static_cast<float>(static_cast<int>(season->currentSeason())) / 3.0f;
    output[1] = season->seasonProgress();
}

}  // namespace neuro
