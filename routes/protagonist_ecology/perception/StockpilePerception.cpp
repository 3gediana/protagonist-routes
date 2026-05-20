#include "perception/StockpilePerception.h"

#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "world/BaseLayer.h"
#include "world/NurseryLogisticsLayer.h"

#include <algorithm>

namespace neuro::routes::protagonist {

void StockpilePerception::sense(const IAgent& self, const IWorld& world, std::span<float> out) const {
    if (out.size() < kDim) {
        return;
    }
    std::fill(out.begin(), out.end(), 0.0f);

    const auto* base = world.getLayer<BaseLayer>();
    const auto* logistics = world.getLayer<NurseryLogisticsLayer>();
    if (base == nullptr || logistics == nullptr) {
        return;
    }

    const float scale = normalization_scale_ > 0.0f ? normalization_scale_ : 1.0f;
    out[0] = std::clamp(static_cast<float>(logistics->stockpiledCount(ObjectType::Stick)) / scale, 0.0f, 1.0f);
    out[1] = std::clamp(static_cast<float>(logistics->stockpiledCount(ObjectType::Stone)) / scale, 0.0f, 1.0f);
    out[2] = std::clamp(static_cast<float>(logistics->stockpiledCount(ObjectType::Spear)) / scale, 0.0f, 1.0f);
    out[3] = base->inBase(self.position()) ? 1.0f : 0.0f;
}

}  // namespace neuro::routes::protagonist
