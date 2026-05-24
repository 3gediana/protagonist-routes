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

    // L5 R3: Lookout building bonus - effective sense radius scaled.
    const auto* self_concrete_for_scale = dynamic_cast<const Agent*>(&self);
    const float scale = self_concrete_for_scale != nullptr ? self_concrete_for_scale->senseRadiusScale() : 1.0f;
    const float effective_radius = sense_radius_ * scale;
    const float effective_r2 = effective_radius * effective_radius;

    const Agent* nearest_prey = nullptr;
    const Agent* nearest_predator = nullptr;
    float nearest_prey_dist_sq = effective_r2;
    float nearest_predator_dist_sq = effective_r2;
    std::size_t peer_count = 0;
    std::size_t predator_count = 0;

    // L2.1 v2 (SPECIES_DIFFERENTIATION_SPEC.md § 1.4): stealth + cave
    // line-of-sight filter. A stealth attacker is invisible to anyone
    // not directly adjacent (< 2m); a prey hiding in a cave is
    // invisible to anyone outside the cave. Both gates are
    // perception-side only - the attacker's hunt capability still
    // checks alive() and proximity, so a brain that randomly walks
    // into a stealth target can still hit it.
    constexpr float kStealthAdjacentRadius = 2.0f;
    constexpr float kStealthAdjacentSq = kStealthAdjacentRadius * kStealthAdjacentRadius;
    const auto* self_concrete = dynamic_cast<const Agent*>(&self);
    const bool self_in_cave = self_concrete != nullptr && self_concrete->isInCave();

    for (const auto* other : agent_layer->agents()) {
        if (other == nullptr || !other->alive() || other->id() == self.id()) {
            continue;
        }
        const auto* concrete = dynamic_cast<const Agent*>(other);
        if (concrete == nullptr) {
            continue;
        }

        const float dist_sq = Vec2::distanceSquared(other->position(), self.position());
        if (dist_sq > effective_r2) {
            continue;
        }

        // Cave hide: prey/peer hiding inside a cave is invisible to
        // observers that are outside the cave. Symmetric for any kind
        // of role - a wolf inside a cave is also invisible to a deer
        // outside, but in practice predators rarely hide.
        if (concrete->isInCave() && !self_in_cave) {
            continue;
        }

        // Stealth: invisible unless observer is directly adjacent.
        if (concrete->isStealth() && dist_sq > kStealthAdjacentSq) {
            continue;
        }

        if (concrete->isPredator()) {
            ++predator_count;
            if (nearest_predator == nullptr || dist_sq < nearest_predator_dist_sq) {
                nearest_predator = concrete;
                nearest_predator_dist_sq = dist_sq;
            }
            continue;
        }

        const bool is_protagonist = dynamic_cast<const ProtagonistBrain*>(&concrete->brain()) != nullptr;
        if (is_protagonist) {
            ++peer_count;
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

    // Local social context: how many protagonist peers and how many
    // predators are in sense_radius. Used by the protagonist brain to
    // pick fight-back vs flee (emergent shelter L1 follow-up).
    out[8] = std::clamp(static_cast<float>(peer_count) / kCountCap, 0.0f, 1.0f);
    out[9] = std::clamp(static_cast<float>(predator_count) / kCountCap, 0.0f, 1.0f);
}

}  // namespace neuro::routes::protagonist
