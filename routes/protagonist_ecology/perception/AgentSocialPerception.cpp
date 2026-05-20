#include "perception/AgentSocialPerception.h"

#include "brain/ProtagonistBrain.h"
#include "core/agent/Agent.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"

#include <algorithm>

namespace neuro::routes::protagonist {

void AgentSocialPerception::sense(const IAgent& self, const IWorld& world, std::span<float> out) const {
    if (out.size() < kDim) {
        return;
    }
    std::fill(out.begin(), out.end(), 0.0f);

    const auto* agent_layer = world.getLayer<AgentLayer>();
    if (agent_layer == nullptr) {
        out[2] = 1.0f;
        out[6] = 1.0f;
        return;
    }

    const Agent* nearest_prey = nullptr;
    const Agent* nearest_predator = nullptr;
    float nearest_prey_dist_sq = sense_radius_ * sense_radius_;
    float nearest_predator_dist_sq = sense_radius_ * sense_radius_;

    for (const auto* other : agent_layer->agents()) {
        if (other == nullptr || !other->alive() || other->id() == self.id()) {
            continue;
        }
        const auto* concrete = dynamic_cast<const Agent*>(other);
        if (concrete == nullptr) {
            continue;
        }

        const float dist_sq = Vec2::distanceSquared(other->position(), self.position());
        if (dist_sq > sense_radius_ * sense_radius_) {
            continue;
        }

        if (concrete->isPredator()) {
            if (nearest_predator == nullptr || dist_sq < nearest_predator_dist_sq) {
                nearest_predator = concrete;
                nearest_predator_dist_sq = dist_sq;
            }
            continue;
        }

        const bool is_protagonist = dynamic_cast<const ProtagonistBrain*>(&concrete->brain()) != nullptr;
        if (is_protagonist) {
            continue;
        }

        if (nearest_prey == nullptr || dist_sq < nearest_prey_dist_sq) {
            nearest_prey = concrete;
            nearest_prey_dist_sq = dist_sq;
        }
    }

    if (nearest_prey != nullptr) {
        const Vec2 delta = nearest_prey->position() - self.position();
        const float dist = delta.length();
        if (dist > 0.0001f) {
            out[0] = delta.x / dist;
            out[1] = delta.y / dist;
        }
        out[2] = std::clamp(dist / (sense_radius_ > 0.0f ? sense_radius_ : 1.0f), 0.0f, 1.0f);
        out[3] = 1.0f;
    } else {
        out[2] = 1.0f;
    }

    if (nearest_predator != nullptr) {
        const Vec2 delta = nearest_predator->position() - self.position();
        const float dist = delta.length();
        if (dist > 0.0001f) {
            out[4] = delta.x / dist;
            out[5] = delta.y / dist;
        }
        out[6] = std::clamp(dist / (sense_radius_ > 0.0f ? sense_radius_ : 1.0f), 0.0f, 1.0f);
        out[7] = 1.0f;
    } else {
        out[6] = 1.0f;
    }
}

}  // namespace neuro::routes::protagonist
