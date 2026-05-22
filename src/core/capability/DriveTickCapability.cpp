#include "core/capability/DriveTickCapability.h"

#include "core/agent/Agent.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"

#include <algorithm>
#include <cmath>

namespace neuro {

DriveTickCapability::DriveTickCapability(float sense_radius,
                                         float metabolism,
                                         Coefficients coeffs)
    : sense_radius_(sense_radius), metabolism_(metabolism), coeffs_(coeffs) {}

void DriveTickCapability::apply(IAgent& agent,
                                IWorld& world,
                                std::span<const float> /*outputs*/,
                                float dt) {
    auto* concrete = dynamic_cast<Agent*>(&agent);
    if (concrete == nullptr) return;
    auto& drives = concrete->driveState();

    // hunger: passive metabolic growth, decremented by food eaten this
    // tick (Agent.foodEatenThisTick() is reset at beginTick, populated
    // by Eat / hunt capabilities, then consumed here at the end of the
    // capability stack).
    drives.hunger += coeffs_.hunger_growth_per_sec * metabolism_ * dt;
    drives.hunger -= coeffs_.hunger_drop_per_food_unit * concrete->foodEatenThisTick();

    // fatigue: integrate |velocity| as activity proxy. Below the
    // rest_speed_threshold the agent is considered resting and fatigue
    // decays toward 0.
    const auto v = concrete->velocity();
    const float speed = std::sqrt(v.x * v.x + v.y * v.y);
    if (speed > coeffs_.fatigue_rest_speed_threshold) {
        drives.fatigue += coeffs_.fatigue_growth_per_speed_per_sec * speed * dt;
    } else {
        drives.fatigue -= coeffs_.fatigue_rest_decay_per_sec * dt;
    }

    // fear / loneliness: scan AgentLayer once for predators / peers in
    // sense radius. This mirrors what AgentSocialPerception does, so
    // for protagonist agents (which already have AgentSocialPerception)
    // there's a small redundant scan. The scan is O(N) and N is at most
    // a few dozen so the cost is negligible.
    bool predator_in_radius = false;
    bool peer_in_radius = false;
    if (const auto* agent_layer = world.getLayer<AgentLayer>(); agent_layer != nullptr) {
        const float radius_sq = sense_radius_ * sense_radius_;
        for (const auto* other : agent_layer->agents()) {
            if (other == nullptr || other == &agent || !other->alive()) continue;
            const auto* other_concrete = dynamic_cast<const Agent*>(other);
            if (other_concrete == nullptr) continue;
            const float dx = other->position().x - agent.position().x;
            const float dy = other->position().y - agent.position().y;
            const float dist_sq = dx * dx + dy * dy;
            if (dist_sq > radius_sq) continue;
            if (other_concrete->isPredator()) {
                predator_in_radius = true;
            } else if (!concrete->isPredator()) {
                // Peer = same-side (non-predator). For predators we treat
                // any non-predator as peer (apex sees apex / apex sees other
                // predator alike); but apex setpoint is high (lonely apex
                // is comfortable) so the semantics still hold.
                peer_in_radius = true;
            } else {
                // Self is predator, other is non-predator: NOT a peer
                // (it's prey).
            }
        }
    }

    if (predator_in_radius) {
        // Pulse on entry would require tracking previous_state; cheap
        // approximation: continuous growth. Larger pulse = quicker
        // dominance once a predator is visible.
        drives.fear += coeffs_.fear_predator_growth_per_sec * dt;
    } else {
        drives.fear -= coeffs_.fear_decay_per_sec * dt;
    }

    if (peer_in_radius) {
        drives.loneliness -= coeffs_.loneliness_peer_drop_per_sec * dt;
    } else {
        drives.loneliness += coeffs_.loneliness_alone_growth_per_sec * dt;
    }

    // Clamp all drives to [0, 1].
    drives.hunger     = std::clamp(drives.hunger,     0.0f, 1.0f);
    drives.fatigue    = std::clamp(drives.fatigue,    0.0f, 1.0f);
    drives.fear       = std::clamp(drives.fear,       0.0f, 1.0f);
    drives.loneliness = std::clamp(drives.loneliness, 0.0f, 1.0f);
}

}  // namespace neuro
