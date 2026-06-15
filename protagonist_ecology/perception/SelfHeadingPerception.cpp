#include "perception/SelfHeadingPerception.h"

#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"

#include <cmath>

namespace neuro::routes::protagonist {

void SelfHeadingPerception::sense(const IAgent& self, const IWorld& /*world*/,
                                  std::span<float> out) const {
    if (out.size() < kDim) {
        return;
    }
    const Vec2 v = self.velocity();
    const float speed = std::sqrt(v.x * v.x + v.y * v.y);
    if (speed > 0.01f) {
        last_cos_ = v.x / speed;   // cos(theta) = vx/|v|
        last_sin_ = v.y / speed;   // sin(theta) = vy/|v|
        has_prev_ = true;
    }
    // else: hold previous heading (or default +x facing if never moved).
    out[0] = last_sin_;
    out[1] = last_cos_;
}

}  // namespace neuro::routes::protagonist