#include "perception/DangerPerception.h"

#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "world/CampfireLayer.h"

#include <algorithm>
#include <cmath>

namespace neuro::routes::protagonist {

void DangerPerception::sense(const IAgent& self, const IWorld& world, std::span<float> out) const {
    if (out.size() < kDim) {
        return;
    }

    const auto* fire = world.getLayer<CampfireLayer>();
    if (fire == nullptr) {
        std::fill(out.begin(), out.end(), 0.0f);
        return;
    }

    const auto nearest = fire->nearestFire(self.position());
    if (!nearest.has_value()) {
        std::fill(out.begin(), out.end(), 0.0f);
        return;
    }

    const Vec2 delta = nearest->position - self.position();
    const float distance = delta.length();
    if (distance > 0.0001f) {
        out[0] = delta.x / distance;
        out[1] = delta.y / distance;
    } else {
        out[0] = 0.0f;
        out[1] = 0.0f;
    }

    out[2] = std::clamp(distance / (sense_radius_ > 0.0f ? sense_radius_ : 1.0f), 0.0f, 1.0f);
    out[3] = fire->inWarmthZone(self.position()) ? 1.0f : 0.0f;
    out[4] = fire->inDangerZone(self.position()) ? 1.0f : 0.0f;
}

}  // namespace neuro::routes::protagonist
