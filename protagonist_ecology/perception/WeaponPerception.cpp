#include "perception/WeaponPerception.h"

#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/object/ObjectLayer.h"

#include <algorithm>

namespace neuro::routes::protagonist {

void WeaponPerception::sense(const IAgent& self, const IWorld& world, std::span<float> out) const {
    if (out.size() < kDim) {
        return;
    }
    std::fill(out.begin(), out.end(), 0.0f);

    const auto* objects = world.getLayer<ObjectLayer>();
    if (objects == nullptr) {
        out[4] = 1.0f;
        return;
    }

    const auto nearby = objects->nearestObjects(self.position(), sense_radius_);
    const MovableObject* best = nullptr;
    float best_dist_sq = 0.0f;
    for (const auto* object : nearby) {
        if (object == nullptr) {
            continue;
        }
        if (object->type != ObjectType::Spear && object->type != ObjectType::Stone) {
            continue;
        }
        const float dist_sq = Vec2::distanceSquared(object->pos, self.position());
        if (best == nullptr || dist_sq < best_dist_sq) {
            best = object;
            best_dist_sq = dist_sq;
        }
    }

    if (best == nullptr) {
        out[4] = 1.0f;
        return;
    }

    const Vec2 delta = best->pos - self.position();
    const float dist = delta.length();
    if (dist > 0.0001f) {
        out[0] = delta.x / dist;
        out[1] = delta.y / dist;
    }
    out[2] = std::clamp(dist / (sense_radius_ > 0.0f ? sense_radius_ : 1.0f), 0.0f, 1.0f);
    out[3] = best->type == ObjectType::Spear ? 1.0f : 0.0f;
    out[4] = 0.0f;
    out[5] = best->heavy ? 1.0f : 0.0f;
}

}  // namespace neuro::routes::protagonist
