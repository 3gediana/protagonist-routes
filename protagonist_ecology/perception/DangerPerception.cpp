#include "perception/DangerPerception.h"

#include "core/agent/Agent.h"
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

    // L5 R3 #7: Lookout sense-radius scaling for danger (fire) perception.
    const auto* concrete = dynamic_cast<const Agent*>(&self);
    const float scale = concrete != nullptr ? concrete->senseRadiusScale() : 1.0f;
    const float effective_radius = sense_radius_ * scale;

    // Phase C limited perception: if the nearest fire is outside the view cone
    // or occluded by terrain, the agent does not currently SEE where it is, so
    // the bearing/distance signals are written as not-sensed (zeros). The
    // proprioceptive warmth/danger-zone flags below are kept: heat is felt even
    // when the source can't be seen. This forces the brain to remember the
    // fire's location and anticipate when it must return, rather than relying on
    // an always-on beacon.
    const bool fire_seen = fovPerceivable(self, nearest->position, world, fov_);

    const Vec2 delta = nearest->position - self.position();
    const float distance = delta.length();
    if (fire_seen && distance > 0.0001f) {
        out[0] = delta.x / distance;
        out[1] = delta.y / distance;
    } else {
        out[0] = 0.0f;
        out[1] = 0.0f;
    }

    out[2] = fire_seen
                 ? std::clamp(distance / (effective_radius > 0.0f ? effective_radius : 1.0f), 0.0f, 1.0f)
                 : 0.0f;
    out[3] = fire->inWarmthZone(self.position()) ? 1.0f : 0.0f;
    out[4] = fire->inDangerZone(self.position()) ? 1.0f : 0.0f;
}

}  // namespace neuro::routes::protagonist
