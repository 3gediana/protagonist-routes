#include "core/perception/InfectionPerception.h"

#include "core/agent/Agent.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"

#include <cmath>

namespace neuro {

InfectionPerception::InfectionPerception(float perception_radius)
    : perception_radius_(perception_radius) {}

void InfectionPerception::sense(const IAgent& agent, const IWorld& world, std::span<float> output) const {
    static constexpr std::size_t kDim = 5;
    if (output.size() < kDim) return;

    const auto* agent_layer = world.getLayer<AgentLayer>();
    if (agent_layer == nullptr) {
        output[0] = 0.0f;
        output[1] = 0.0f;
        output[2] = 0.0f;
        output[3] = 0.0f;
        output[4] = 0.0f;
        return;
    }

    const auto* concrete = dynamic_cast<const Agent*>(&agent);
    const bool self_infected = concrete ? concrete->infected() : false;

    std::size_t infected_nearby = 0;
    std::size_t total_nearby = 0;
    float nearest_dx = 0.0f;
    float nearest_dy = 0.0f;
    float min_dist = perception_radius_ + 1.0f;

    for (const auto* other : agent_layer->agents()) {
        if (other == &agent || !other->alive()) continue;

        const float dx = other->position().x - agent.position().x;
        const float dy = other->position().y - agent.position().y;
        const float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < perception_radius_) {
            total_nearby++;
            const auto* other_concrete = dynamic_cast<const Agent*>(other);
            if (other_concrete && other_concrete->infected()) {
                infected_nearby++;
                if (dist < min_dist) {
                    min_dist = dist;
                    nearest_dx = dx;
                    nearest_dy = dy;
                }
            }
        }
    }

    output[0] = self_infected ? 1.0f : 0.0f;
    output[1] = total_nearby > 0 ? static_cast<float>(infected_nearby) / static_cast<float>(total_nearby) : 0.0f;
    output[2] = infected_nearby > 0 ? nearest_dx / perception_radius_ : 0.0f;
    output[3] = infected_nearby > 0 ? nearest_dy / perception_radius_ : 0.0f;
    // Immune memory richness: how many strains recovered from (capped at 5)
    output[4] = concrete ? std::min(static_cast<float>(concrete->immuneMemory().size()), 5.0f) / 5.0f : 0.0f;
}

}  // namespace neuro
