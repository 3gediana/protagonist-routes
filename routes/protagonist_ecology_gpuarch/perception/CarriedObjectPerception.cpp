#include "perception/CarriedObjectPerception.h"

#include "core/agent/Agent.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/object/ObjectLayer.h"

#include <algorithm>

namespace neuro::routes::protagonist {

void CarriedObjectPerception::sense(const IAgent& self, const IWorld& world, std::span<float> out) const {
    if (out.size() < kDim) {
        return;
    }
    std::fill(out.begin(), out.end(), 0.0f);

    const auto* concrete = dynamic_cast<const Agent*>(&self);
    const auto* objects = world.getLayer<ObjectLayer>();
    if (concrete == nullptr || objects == nullptr || !concrete->isCarrying()) {
        return;
    }

    out[0] = 1.0f;
    const auto* carried = objects->findById(concrete->carriedObject());
    if (carried == nullptr) {
        return;
    }

    if (carried->type == ObjectType::Stick) {
        out[1] = 1.0f;
    } else if (carried->type == ObjectType::Stone) {
        out[2] = 1.0f;
    } else if (carried->type == ObjectType::Spear) {
        out[3] = 1.0f;
    }
}

}  // namespace neuro::routes::protagonist
