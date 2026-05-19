#include "core/capability/FleeCapability.h"
#include "core/agent/Agent.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"
#include "core/world/layers/TimeOfDayLayer.h"

#include <cmath>

namespace neuro {

FleeCapability::FleeCapability(float flee_radius, float flee_speed_boost, float flee_cost)
    : flee_radius_(flee_radius), flee_speed_boost_(flee_speed_boost), flee_cost_(flee_cost) {}

std::size_t FleeCapability::consumedOutputs() const {
    return 1;  // flee intensity [0,1]
}

void FleeCapability::apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) {
    if (outputs.empty() || outputs[0] < 0.3f) return;

    auto* concrete = dynamic_cast<Agent*>(&agent);
    if (concrete == nullptr) return;
    if (concrete->isPredator()) return;  // predators don't flee

    auto* agent_layer = world.getLayer<AgentLayer>();
    if (agent_layer == nullptr) return;

    // Find nearest predator
    float nearest_dx = 0.0f;
    float nearest_dy = 0.0f;
    float nearest_dist = flee_radius_;
    bool threat_found = false;

    for (const auto* other : agent_layer->agents()) {
        if (other == &agent || !other->alive()) continue;
        const auto* other_concrete = dynamic_cast<const Agent*>(other);
        if (other_concrete == nullptr || !other_concrete->isPredator()) continue;

        const float dx = agent.position().x - other->position().x;
        const float dy = agent.position().y - other->position().y;
        const float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < nearest_dist) {
            nearest_dist = dist;
            nearest_dx = dx;
            nearest_dy = dy;
            threat_found = true;
        }
    }

    if (!threat_found) return;

    // Day/night: prey flee more effectively during day (1.0x), worse at night (0.6x)
    float time_mult = 1.0f;
    const auto* tod = world.getLayer<TimeOfDayLayer>();
    if (tod != nullptr) {
        const float light = tod->lightLevel();
        time_mult = 0.6f + 0.4f * light;
    }

    // Flee direction: away from predator
    const float len = std::sqrt(nearest_dx * nearest_dx + nearest_dy * nearest_dy);
    if (len < 0.001f) return;

    const float flee_intensity = outputs[0];
    const float boost = flee_speed_boost_ * flee_intensity * time_mult;
    Vec2 flee_dir{nearest_dx / len, nearest_dy / len};

    // Apply flee velocity boost on top of normal movement
    Vec2 flee_velocity = flee_dir * boost;
    Vec2 new_pos = agent.position() + flee_velocity * dt;

    // Clamp to world bounds
    const Vec2 ws = world.size();
    new_pos.x = std::clamp(new_pos.x, 0.0f, ws.x);
    new_pos.y = std::clamp(new_pos.y, 0.0f, ws.y);

    agent.setPosition(new_pos);
    concrete->setEnergy(concrete->energy() - flee_cost_ * flee_intensity * dt);
}

const char* FleeCapability::name() const {
    return "flee";
}

}  // namespace neuro
