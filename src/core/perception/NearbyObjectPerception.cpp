#include "core/perception/NearbyObjectPerception.h"

#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/object/ObjectLayer.h"

#include <cmath>

namespace neuro {

NearbyObjectPerception::NearbyObjectPerception(float perception_radius)
    : perception_radius_(perception_radius) {}

void NearbyObjectPerception::sense(const IAgent& agent, const IWorld& world, std::span<float> inputs) const {
    if (inputs.size() < kDim) return;

    auto* obj_layer = world.getLayer<ObjectLayer>();
    if (obj_layer == nullptr) {
        for (std::size_t i = 0; i < kDim; ++i) inputs[i] = 0.0f;
        return;
    }

    auto nearby = obj_layer->nearestObjects(agent.position(), perception_radius_);
    if (nearby.empty()) {
        for (std::size_t i = 0; i < kDim; ++i) inputs[i] = 0.0f;
        return;
    }

    const auto& nearest = nearby[0];
    const float dx = nearest->pos.x - agent.position().x;
    const float dy = nearest->pos.y - agent.position().y;
    const float dist = std::sqrt(dx * dx + dy * dy);

    inputs[0] = dx / perception_radius_;  // normalized dx
    inputs[1] = dy / perception_radius_;  // normalized dy
    inputs[2] = dist / perception_radius_;  // normalized distance
    inputs[3] = static_cast<float>(nearest->type) / static_cast<float>(ObjectType::Count);  // type onehot
}

}  // namespace neuro
