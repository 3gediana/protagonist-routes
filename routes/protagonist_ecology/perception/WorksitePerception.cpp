#include "perception/WorksitePerception.h"

#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "world/WorksiteLayer.h"

#include <algorithm>

namespace neuro::routes::protagonist {

void WorksitePerception::sense(const IAgent& self, const IWorld& world, std::span<float> out) const {
    if (out.size() < kDim) {
        return;
    }

    std::fill(out.begin(), out.end(), 0.0f);
    const auto* worksites = world.getLayer<WorksiteLayer>();
    if (worksites == nullptr) {
        return;
    }

    const auto nearest = worksites->nearestActiveSite(self.position());
    if (!nearest.has_value()) {
        out[4] = 1.0f;
        return;
    }

    const Vec2 delta = nearest->position - self.position();
    const float dist = delta.length();
    if (require_visibility_ && dist > sense_radius_) {
        out[4] = 1.0f;
        return;
    }
    if (dist > 0.0001f) {
        out[0] = delta.x / dist;
        out[1] = delta.y / dist;
    }
    out[2] = std::clamp(dist / (sense_radius_ > 0.0f ? sense_radius_ : 1.0f), 0.0f, 1.0f);
    out[3] = worksites->inActiveSite(self.position()) ? 1.0f : 0.0f;
    out[4] = nearest->completed ? 1.0f : 0.0f;
    out[5] = nearest->build_progress;
    const float remaining_sticks = static_cast<float>(nearest->required_sticks > nearest->stored_sticks ? nearest->required_sticks - nearest->stored_sticks : 0u);
    const float remaining_stones = static_cast<float>(nearest->required_stones > nearest->stored_stones ? nearest->required_stones - nearest->stored_stones : 0u);
    out[6] = nearest->required_sticks > 0 ? remaining_sticks / static_cast<float>(nearest->required_sticks) : 0.0f;
    out[7] = nearest->required_stones > 0 ? remaining_stones / static_cast<float>(nearest->required_stones) : 0.0f;
}

}  // namespace neuro::routes::protagonist
