#include "core/perception/NearestFoodPerception.h"

#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/ResourceLayer.h"

#include <algorithm>
#include <cmath>

namespace neuro {

std::size_t NearestFoodPerception::dim() const {
    return 3;
}

void NearestFoodPerception::sense(const IAgent& self, const IWorld& world, std::span<float> out) const {
    const auto* resources = world.getLayer<ResourceLayer>();
    if (resources == nullptr) {
        out[0] = 0.0f;
        out[1] = 0.0f;
        out[2] = 1.0f;
        return;
    }

    const auto nearest = resources->nearestAvailable(self.position());
    if (!nearest.has_value()) {
        out[0] = 0.0f;
        out[1] = 0.0f;
        out[2] = 1.0f;
        return;
    }

    const Vec2 delta = nearest->position - self.position();
    const float distance = nearest->distance;
    if (distance > 0.0001f) {
        out[0] = delta.x / distance;
        out[1] = delta.y / distance;
    } else {
        out[0] = 0.0f;
        out[1] = 0.0f;
    }

    const float diag = std::sqrt(world.size().x * world.size().x + world.size().y * world.size().y);
    out[2] = std::clamp(distance / (diag > 0.0f ? diag : 1.0f), 0.0f, 1.0f);
}

const char* NearestFoodPerception::name() const {
    return "nearest_food";
}

}  // namespace neuro
