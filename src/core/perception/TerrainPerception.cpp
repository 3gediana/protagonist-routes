#include "core/perception/TerrainPerception.h"

#include "core/agent/Agent.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/terrain/TerrainLayer.h"

#include <cmath>

namespace neuro {

TerrainPerception::TerrainPerception(float height_scale)
    : height_scale_(height_scale > 0.0f ? height_scale : 1.0f) {}

void TerrainPerception::sense(const IAgent& agent, const IWorld& world, std::span<float> output) const {
    auto* terrain = world.getLayer<TerrainLayer>();
    if (terrain == nullptr || output.size() < kDim) {
        for (std::size_t i = 0; i < output.size(); ++i) {
            output[i] = 0.0f;
        }
        return;
    }

    const float x = agent.position().x;
    const float y = agent.position().y;

    output[0] = terrain->heightAt(x, y) / height_scale_;
    const Vec2 slope = terrain->slopeAt(x, y);
    output[1] = slope.x;
    output[2] = slope.y;
    output[3] = terrain->inCave(x, y) ? 1.0f : 0.0f;

    float forward_vis = 1.0f;
    const float look_dist = 20.0f;
    const auto& velocity = agent.velocity();
    const float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
    if (speed > 0.01f) {
        const Vec2 dir{velocity.x / speed, velocity.y / speed};
        const Vec2 target{x + dir.x * look_dist, y + dir.y * look_dist};
        if (!terrain->isVisible(agent.position(), target)) {
            forward_vis = 0.0f;
        }
    }
    output[4] = forward_vis;
}

}  // namespace neuro
