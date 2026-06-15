#include "capability/SocialActionCapability.h"

#include "brain/ProtagonistBrain.h"
#include "core/agent/Agent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"
#include "world/BaseLayer.h"
#include "world/TerritoryMarkLayer.h"

#include <algorithm>
#include <cmath>

namespace neuro::routes::protagonist {

namespace {

const ProtagonistBrain* asProtagonistBrain(const IBrain& brain) {
    return dynamic_cast<const ProtagonistBrain*>(&brain);
}

// Cosine of two genetic fingerprints mapped to [0,1] (same measure as Kin).
float relatednessFromFingerprints(std::span<const float> a, std::span<const float> b) {
    if (a.empty() || a.size() != b.size()) {
        return 0.0f;
    }
    float dot = 0.0f, na = 0.0f, nb = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
        na += a[i] * a[i];
        nb += b[i] * b[i];
    }
    if (na <= 1.0e-12f || nb <= 1.0e-12f) {
        return 0.0f;
    }
    return std::clamp(dot / std::sqrt(na * nb), 0.0f, 1.0f);
}

constexpr float kAllyRelatedness = 0.6f;  // >= this == kin/ally (see Territory)

}  // namespace

void SocialActionCapability::apply(IAgent& agent, IWorld& world,
                                   std::span<const float> outputs, float /*dt*/) {
    auto* self = dynamic_cast<Agent*>(&agent);
    if (self == nullptr) {
        return;
    }
    auto* agent_layer = world.getLayer<AgentLayer>();
    if (agent_layer == nullptr) {
        return;
    }
    const ProtagonistBrain* self_brain = asProtagonistBrain(self->brain());
    if (self_brain == nullptr) {
        return;
    }
    const std::span<const float> self_fp = self_brain->geneticFingerprint();
    const Vec2 pos = self->position();
    const float r2 = sense_radius_ * sense_radius_;
    const bool self_in_cave = self->isInCave();

    // ---- [0] feed-kin (D): give surplus energy to the nearest close relative.
    if (outputs.size() > 0 && outputs[0] >= activation_threshold_) {
        feed_attempts.fetch_add(1, std::memory_order_relaxed);
        const float self_energy = self->energy();
        if (self_energy >= feed_min_surplus_) {  // only share when I'm well-fed
            Agent* nearest_kin = nullptr;
            float best_d2 = r2;
            for (auto* other : agent_layer->agents()) {
                if (other == nullptr || !other->alive() || other->id() == self->id()) {
                    continue;
                }
                if (other->isInCave() && !self_in_cave) {
                    continue;
                }
                if (other->speciesId() != self->speciesId()) {
                    continue;
                }
                const ProtagonistBrain* ob = asProtagonistBrain(other->brain());
                if (ob == nullptr) {
                    continue;
                }
                const float d2 = Vec2::distanceSquared(other->position(), pos);
                if (d2 > r2) {
                    continue;
                }
                if (relatednessFromFingerprints(self_fp, ob->geneticFingerprint()) < kAllyRelatedness) {
                    continue;
                }
                if (d2 < best_d2) {
                    best_d2 = d2;
                    nearest_kin = other;
                }
            }
            if (nearest_kin != nullptr) {
                const float give = std::min(feed_amount_, self_energy - 1.0f);
                if (give > 0.0f) {
                    self->setEnergy(self_energy - give);
                    nearest_kin->setEnergy(nearest_kin->energy() + give);
                    feed_successes.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }
    }

    // ---- [1] mark-territory (E): drop/refresh my own scent-mark on the ground
    // here. Real effect recorded in TerritoryMarkLayer, real energy cost, with
    // causal gates: needs a minimum energy to spend, and re-marking on top of an
    // existing owned mark refreshes it instead of spamming duplicates.
    if (outputs.size() > 1 && outputs[1] >= activation_threshold_) {
        mark_attempts.fetch_add(1, std::memory_order_relaxed);
        auto* territory = world.getLayer<TerritoryMarkLayer>();
        const float self_energy = self->energy();
        if (territory != nullptr && self_energy >= mark_min_energy_ && !self_fp.empty()) {
            if (territory->placeOrRefresh(pos, self_fp)) {
                self->setEnergy(self_energy - mark_energy_cost_);
                mark_successes.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }

    // ---- [2] evict-intruder (E): shove the nearest non-kin off land I hold.
    // "Land I hold" = my colony base OR territory I've marked as mine. This is
    // the payoff that makes marking matter: mark ground -> you can defend it.
    if (outputs.size() > 2 && outputs[2] >= activation_threshold_) {
        evict_attempts.fetch_add(1, std::memory_order_relaxed);
        const auto* base = world.getLayer<BaseLayer>();
        const auto* territory = world.getLayer<TerritoryMarkLayer>();
        const auto inMyLand = [&](Vec2 p) {
            if (base != nullptr && base->inBase(p)) {
                return true;
            }
            if (territory != nullptr && !self_fp.empty() &&
                territory->inOwnedTerritory(p, self_fp)) {
                return true;
            }
            return false;
        };
        if (inMyLand(pos)) {
            Agent* nearest_intruder = nullptr;
            float best_d2 = r2;
            for (auto* other : agent_layer->agents()) {
                if (other == nullptr || !other->alive() || other->id() == self->id()) {
                    continue;
                }
                if (!inMyLand(other->position())) {
                    continue;  // intruder must be standing on land I hold
                }
                // Non-kin = a co-species protagonist below the ally threshold, or
                // anything that isn't a co-species protagonist at all (predator,
                // prey, other species).
                bool non_kin = true;
                const ProtagonistBrain* ob = asProtagonistBrain(other->brain());
                if (ob != nullptr && other->speciesId() == self->speciesId() && !self_fp.empty()) {
                    non_kin = relatednessFromFingerprints(self_fp, ob->geneticFingerprint()) < kAllyRelatedness;
                }
                if (!non_kin) {
                    continue;
                }
                const float d2 = Vec2::distanceSquared(other->position(), pos);
                if (d2 > r2) {
                    continue;
                }
                if (d2 < best_d2) {
                    best_d2 = d2;
                    nearest_intruder = other;
                }
            }
            if (nearest_intruder != nullptr) {
                // Push outward: away from the base center when the intruder is in
                // my base, otherwise directly away from me (the defender) on the
                // marked ground.
                Vec2 repel_center = pos;
                if (base != nullptr && base->inBase(nearest_intruder->position())) {
                    repel_center = base->baseCenter();
                }
                Vec2 dir = nearest_intruder->position() - repel_center;
                if (dir.lengthSquared() < 1.0e-8f) {
                    dir = Vec2{1.0f, 0.0f};
                }
                const Vec2 newpos = nearest_intruder->position() + dir.normalized() * evict_push_;
                nearest_intruder->setPosition(newpos);
                evict_successes.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }
}

}  // namespace neuro::routes::protagonist
