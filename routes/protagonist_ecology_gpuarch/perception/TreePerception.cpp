#include "perception/TreePerception.h"

#include "core/agent/Agent.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "world/TreeLayer.h"

#include <algorithm>

namespace neuro::routes::protagonist {

void TreePerception::sense(const IAgent& self, const IWorld& world, std::span<float> out) const {
    if (out.size() < kDim) {
        return;
    }
    std::fill(out.begin(), out.end(), 0.0f);

    const auto* trees = world.getLayer<TreeLayer>();
    if (trees == nullptr) {
        out[2] = 1.0f;
        return;
    }

    const auto nearest = trees->nearestTree(self.position());
    if (!nearest.has_value()) {
        out[2] = 1.0f;
        return;
    }

    // L5 R3 #7: Lookout sense-radius scaling. Inside a Lookout building
    // an agent's effective sense_radius for tree perception widens by
    // its senseRadiusScale (set by WorksiteLayer once per tick). Distance
    // normalisation uses the widened radius so "tree at 30m" reads as
    // closer-on-the-input when the agent stands in a Lookout, matching
    // the AgentSocialPerception integration.
    const auto* concrete = dynamic_cast<const Agent*>(&self);
    const float scale = concrete != nullptr ? concrete->senseRadiusScale() : 1.0f;
    const float effective_radius = sense_radius_ * scale;

    const Vec2 delta = nearest->position - self.position();
    const float distance = delta.length();
    if (distance > effective_radius && effective_radius > 0.0f) {
        out[2] = 1.0f;  // beyond effective range — treat as "no tree visible"
        return;
    }
    if (distance > 0.0001f) {
        out[0] = delta.x / distance;
        out[1] = delta.y / distance;
    }
    out[2] = std::clamp(distance / (effective_radius > 0.0f ? effective_radius : 1.0f), 0.0f, 1.0f);
    out[3] = trees->inChopRange(self.position(), chop_radius_) ? 1.0f : 0.0f;
    out[4] = std::clamp(nearest->integrity / (nearest->max_integrity > 0.0f ? nearest->max_integrity : 1.0f), 0.0f, 1.0f);
}

}  // namespace neuro::routes::protagonist
