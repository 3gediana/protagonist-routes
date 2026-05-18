#include "core/reward/CoopReward.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/HeavyResourceLayer.h"

#include <cmath>

namespace neuro {

CoopReward::CoopReward(double weight, double proximity_bonus)
    : weight_(weight), proximity_bonus_(proximity_bonus) {}

double CoopReward::evaluate(const IAgent& agent, const IWorld& world, float dt) const {
    const auto* heavy = world.getLayer<HeavyResourceLayer>();
    if (heavy == nullptr) return 0.0;

    double reward = 0.0;
    for (const auto& resource : heavy->resources()) {
        if (!resource.available) continue;
        const float dx = agent.position().x - resource.position.x;
        const float dy = agent.position().y - resource.position.y;
        const float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < 15.0f) {
            reward += proximity_bonus_ * dt;
            if (resource.current_workers >= resource.required_workers) {
                reward += proximity_bonus_ * 2.0 * dt;
            }
        }
    }

    return reward * weight_;
}

double CoopReward::weight() const { return weight_; }

const char* CoopReward::name() const { return "coop"; }

}  // namespace neuro
