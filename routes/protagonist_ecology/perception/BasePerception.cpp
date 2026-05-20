#include "perception/BasePerception.h"

#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "world/BaseLayer.h"

#include <algorithm>
#include <cmath>

namespace neuro::routes::protagonist {

void BasePerception::sense(const IAgent& self, const IWorld& world, std::span<float> out) const {
    if (out.size() < kDim) return;

    const auto* base = world.getLayer<BaseLayer>();
    if (base == nullptr) {
        std::fill(out.begin(), out.end(), 0.0f);
        return;
    }

    const Vec2 to_base = base->baseCenter() - self.position();
    const float base_dist = to_base.length();
    if (base_dist > 0.0001f) {
        out[0] = to_base.x / base_dist;
        out[1] = to_base.y / base_dist;
    } else {
        out[0] = 0.0f;
        out[1] = 0.0f;
    }

    const Vec2 to_stockpile = base->stockpileCenter() - self.position();
    const float stockpile_dist = to_stockpile.length();
    if (stockpile_dist > 0.0001f) {
        out[6] = to_stockpile.x / stockpile_dist;
        out[7] = to_stockpile.y / stockpile_dist;
    } else {
        out[6] = 0.0f;
        out[7] = 0.0f;
    }

    const Vec2 to_nest = base->nestCenter() - self.position();
    const float nest_dist = to_nest.length();
    if (nest_dist > 0.0001f) {
        out[8] = to_nest.x / nest_dist;
        out[9] = to_nest.y / nest_dist;
    } else {
        out[8] = 0.0f;
        out[9] = 0.0f;
    }

    const float diag = std::sqrt(world.size().x * world.size().x + world.size().y * world.size().y);
    out[2] = std::clamp(base_dist / (diag > 0.0f ? diag : 1.0f), 0.0f, 1.0f);
    out[3] = base->inBase(self.position()) ? 1.0f : 0.0f;
    out[4] = base->inStockpile(self.position()) ? 1.0f : 0.0f;
    out[5] = base->inNest(self.position()) ? 1.0f : 0.0f;
}

}  // namespace neuro::routes::protagonist
