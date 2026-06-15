#include "perception/KinPerception.h"

#include "brain/ProtagonistBrain.h"
#include "core/agent/Agent.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"

#include <algorithm>
#include <cmath>

namespace neuro::routes::protagonist {

namespace {

// A protagonist carries a genetic fingerprint; background species do not, so
// a null result here means "not kin".
const ProtagonistBrain* asProtagonistBrain(const IBrain& brain) {
    return dynamic_cast<const ProtagonistBrain*>(&brain);
}

// Cosine similarity of two genetic fingerprints mapped to a relatedness in
// [0,1]. Unrelated genomes project to ~orthogonal fingerprints (cos ~ 0);
// near-clones project to ~parallel ones (cos ~ 1). Negative cosines (weaker
// than "unrelated") are clamped to 0.
float relatednessFromFingerprints(std::span<const float> a, std::span<const float> b) {
    if (a.empty() || a.size() != b.size()) {
        return 0.0f;
    }
    float dot = 0.0f;
    float na = 0.0f;
    float nb = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
        na += a[i] * a[i];
        nb += b[i] * b[i];
    }
    if (na <= 1.0e-12f || nb <= 1.0e-12f) {
        return 0.0f;
    }
    const float cosine = dot / std::sqrt(na * nb);
    return std::clamp(cosine, 0.0f, 1.0f);
}

}  // namespace

void KinPerception::sense(const IAgent& self, const IWorld& world, std::span<float> out) const {
    if (out.size() < kDim) {
        return;
    }
    std::fill(out.begin(), out.begin() + kDim, 0.0f);

    // Self must be a protagonist to have a fingerprint to compare against.
    const ProtagonistBrain* self_brain = asProtagonistBrain(self.brain());
    if (self_brain == nullptr) {
        return;
    }
    const std::span<const float> self_fp = self_brain->geneticFingerprint();
    if (self_fp.empty()) {
        return;
    }

    const auto* agent_layer = world.getLayer<AgentLayer>();
    if (agent_layer == nullptr) {
        return;
    }

    const auto* self_concrete = dynamic_cast<const Agent*>(&self);
    const bool self_in_cave = self_concrete != nullptr && self_concrete->isInCave();

    const float r2 = sense_radius_ * sense_radius_;
    const Agent* nearest_kin = nullptr;
    float nearest_dist_sq = r2;

    for (const auto* other : agent_layer->agents()) {
        if (other == nullptr || !other->alive() || other->id() == self.id()) {
            continue;
        }
        // Kin = other protagonists. Background species carry no fingerprint.
        if (asProtagonistBrain(other->brain()) == nullptr) {
            continue;
        }
        // Cave hide: a peer inside a cave is invisible to an observer outside
        // (mirror of AgentSocialPerception's line-of-sight gate).
        if (other->isInCave() && !self_in_cave) {
            continue;
        }
        const float dist_sq = Vec2::distanceSquared(other->position(), self.position());
        if (dist_sq > nearest_dist_sq) {
            continue;
        }
        nearest_kin = other;
        nearest_dist_sq = dist_sq;
    }

    if (nearest_kin == nullptr) {
        return;  // no kin in range -> zeros (graceful fallback)
    }

    out[0] = relatednessFromFingerprints(self_fp, asProtagonistBrain(nearest_kin->brain())->geneticFingerprint());

    const Vec2 delta = nearest_kin->position() - self.position();
    const float dist = delta.length();
    if (dist > 1.0e-4f) {
        out[1] = delta.x / dist;
        out[2] = delta.y / dist;
    }
}

}  // namespace neuro::routes::protagonist
