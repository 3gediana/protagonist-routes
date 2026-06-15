#include "perception/ObjectLogisticsPerception.h"

#include "core/agent/Agent.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/object/ObjectLayer.h"
#include "world/BaseLayer.h"

#include <algorithm>

namespace neuro::routes::protagonist {

ObjectLogisticsPerception::ObjectLogisticsPerception(float sense_radius)
    : sense_radius_(sense_radius) {}

void ObjectLogisticsPerception::sense(const IAgent& self, const IWorld& world, std::span<float> out) const {
    if (out.size() < kDim) return;
    std::fill(out.begin(), out.end(), 0.0f);

    const auto* objects = world.getLayer<ObjectLayer>();
    const auto* base = world.getLayer<BaseLayer>();
    if (objects == nullptr) {
        return;
    }

    // L5 R3 #7: Lookout sense-radius scaling for object logistics
    // perception. Querying ObjectLayer with effective_radius widens the
    // candidate pool inside Lookout (more sticks/stones visible).
    const auto* self_concrete = dynamic_cast<const Agent*>(&self);
    const float scale = self_concrete != nullptr ? self_concrete->senseRadiusScale() : 1.0f;
    const float effective_radius = sense_radius_ * scale;

    const auto nearby = objects->nearestObjects(self.position(), effective_radius);
    if (nearby.empty()) {
        out[4] = 1.0f;
        return;
    }

    const auto* object = nearby.front();

    const Vec2 delta = object->pos - self.position();
    const float dist = delta.length();
    if (dist > 0.0001f) {
        out[0] = delta.x / dist;
        out[1] = delta.y / dist;
    }
    out[2] = std::clamp(dist / (effective_radius > 0.0f ? effective_radius : 1.0f), 0.0f, 1.0f);
    out[3] = object->heavy ? 1.0f : 0.0f;
    out[4] = 0.0f;
    out[5] = object->type == ObjectType::Stone ? 1.0f : 0.0f;
    out[6] = object->type == ObjectType::Spear ? 1.0f : 0.0f;
    out[7] = (base != nullptr && base->inStockpile(object->pos)) ? 1.0f : 0.0f;

    if (const auto* concrete = dynamic_cast<const Agent*>(&self); concrete != nullptr) {
        out[8] = concrete->isCarrying() ? 1.0f : 0.0f;
    }
}

}  // namespace neuro::routes::protagonist
