#include "core/perception/FoodScentPerception.h"

#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/ResourceLayer.h"

#include <cmath>

namespace neuro {

void FoodScentPerception::sense(const IAgent& agent, const IWorld& world, std::span<float> output) const {
    if (output.size() < kDim) return;

    const auto* resources = world.getLayer<ResourceLayer>();
    if (resources == nullptr) {
        output[0] = 0.0f;
        output[1] = 0.0f;
        return;
    }

    const auto nearest = resources->nearestAvailable(agent.position());
    if (!nearest.has_value()) {
        output[0] = 0.0f;
        output[1] = 0.0f;
        return;
    }

    // Output: coarse direction to food (dx, dy normalized to [-1,1])
    // This is a "scent gradient" — tells you which direction food is,
    // but not exactly where. Agents need signals for precise navigation.
    const float dx = nearest->position.x - agent.position().x;
    const float dy = nearest->position.y - agent.position().y;
    const float dist = std::sqrt(dx * dx + dy * dy);

    if (dist < 0.001f) {
        output[0] = 0.0f;
        output[1] = 0.0f;
        return;
    }

    // Normalize direction to [-1, 1], scale by proximity (closer = stronger scent)
    const float max_range = 50.0f;
    const float strength = std::clamp(1.0f - dist / max_range, 0.0f, 1.0f);
    output[0] = (dx / dist) * strength;
    output[1] = (dy / dist) * strength;
}

const char* FoodScentPerception::name() const {
    return "food_scent";
}

}  // namespace neuro
