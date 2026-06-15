#include "perception/BorderPerception.h"

#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"

#include <algorithm>

namespace neuro::routes::protagonist {

namespace {
// The closeness ramps saturate at 10% of the smaller world dimension, so the
// signal is 0 across most of the map and rises smoothly within the border band.
constexpr float kBandFraction = 0.1f;
}  // namespace

void BorderPerception::sense(const IAgent& self, const IWorld& world,
                             std::span<float> out) const {
    if (out.size() < kDim) {
        return;
    }
    std::fill(out.begin(), out.end(), 0.0f);

    const float w = world.size().x;
    const float h = world.size().y;
    if (w <= 0.0f || h <= 0.0f) {
        return;
    }
    const float x = std::clamp(self.position().x, 0.0f, w);
    const float y = std::clamp(self.position().y, 0.0f, h);

    out[0] = 2.0f * x / w - 1.0f;
    out[1] = 2.0f * y / h - 1.0f;

    const float band = kBandFraction * std::min(w, h);
    if (band > 0.0f) {
        const float dx = std::min(x, w - x);
        const float dy = std::min(y, h - y);
        out[2] = std::clamp(1.0f - dx / band, 0.0f, 1.0f);
        out[3] = std::clamp(1.0f - dy / band, 0.0f, 1.0f);
    }
}

}  // namespace neuro::routes::protagonist
