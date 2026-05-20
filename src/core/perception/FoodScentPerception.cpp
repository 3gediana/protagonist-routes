#include "core/perception/FoodScentPerception.h"

#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/ResourceLayer.h"

#include <algorithm>
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

    const auto& all_resources = resources->resources();
    if (all_resources.empty()) {
        output[0] = 0.0f;
        output[1] = 0.0f;
        return;
    }

    constexpr float kSmellRadius = 50.0f;
    constexpr float kLocalRadius = 16.0f;

    float nearest_proximity = 0.0f;
    float local_density = 0.0f;

    for (const auto& resource : all_resources) {
        if (!resource.available) {
            continue;
        }

        const float dx = resource.position.x - agent.position().x;
        const float dy = resource.position.y - agent.position().y;
        const float dist_sq = dx * dx + dy * dy;
        const float dist = std::sqrt(dist_sq);
        if (dist > kSmellRadius) {
            continue;
        }

        const float proximity = std::clamp(1.0f - dist / kSmellRadius, 0.0f, 1.0f);
        nearest_proximity = std::max(nearest_proximity, proximity);

        if (dist <= kLocalRadius) {
            const float local_weight = std::clamp(1.0f - dist / kLocalRadius, 0.0f, 1.0f);
            local_density += local_weight;
        }
    }

    output[0] = nearest_proximity;
    output[1] = 1.0f - std::exp(-0.75f * local_density);
}

const char* FoodScentPerception::name() const {
    return "food_scent";
}

}  // namespace neuro
