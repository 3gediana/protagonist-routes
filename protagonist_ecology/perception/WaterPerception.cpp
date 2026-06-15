#include "perception/WaterPerception.h"

#include "core/agent/Agent.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "world/WaterLayer.h"

#include <algorithm>

namespace neuro::routes::protagonist {

void WaterPerception::sense(const IAgent& self, const IWorld& world, std::span<float> out) const {
    if (out.size() < kDim) {
        return;
    }
    std::fill(out.begin(), out.end(), 0.0f);

    const auto* water = world.getLayer<WaterLayer>();
    if (water == nullptr) {
        out[2] = 1.0f;
        return;
    }

    const auto nearest = water->nearestWater(self.position());
    if (!nearest.has_value()) {
        out[2] = 1.0f;
        return;
    }

    // L5 R3 #7: Lookout sense-radius scaling for water perception.
    const auto* concrete = dynamic_cast<const Agent*>(&self);
    const float scale = concrete != nullptr ? concrete->senseRadiusScale() : 1.0f;
    const float effective_radius = sense_radius_ * scale;

    const Vec2 delta = nearest->position - self.position();
    const float distance = delta.length();
    if (require_visibility_ && distance > effective_radius) {
        out[2] = 1.0f;
        return;
    }
    if (distance > 0.0001f) {
        out[0] = delta.x / distance;
        out[1] = delta.y / distance;
    }
    out[2] = std::clamp(distance / (effective_radius > 0.0f ? effective_radius : 1.0f), 0.0f, 1.0f);
    out[3] = water->inDrinkZone(self.position()) ? 1.0f : 0.0f;
    out[4] = 1.0f;
}

}  // namespace neuro::routes::protagonist
