#include "perception/SurvivalStatePerception.h"

#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "world/SurvivalStatusLayer.h"

#include <algorithm>

namespace neuro::routes::protagonist {

void SurvivalStatePerception::sense(const IAgent& self, const IWorld& world, std::span<float> out) const {
    if (out.size() < kDim) {
        return;
    }

    std::fill(out.begin(), out.end(), 0.0f);
    const auto* survival = world.getLayer<SurvivalStatusLayer>();
    if (survival == nullptr) {
        out[0] = 1.0f;
        out[1] = 1.0f;
        return;
    }

    const float hydration = survival->hydrationRatio(self.id());
    const float health = survival->healthRatio(self.id());
    out[0] = hydration;
    out[1] = health;
    out[2] = survival->isDehydrated(self.id()) ? 1.0f : 0.0f;
    out[3] = survival->isInjured(self.id()) ? 1.0f : 0.0f;
    out[4] = 1.0f - hydration;
    out[5] = 1.0f - health;
}

}  // namespace neuro::routes::protagonist
