#include "perception/TerritorySocialPerception.h"

#include "brain/ProtagonistBrain.h"
#include "core/agent/Agent.h"
#include "core/interfaces/IAgent.h"
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

// Cosine similarity of two genetic fingerprints mapped to [0,1] (see Kin).
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

}  // namespace

void TerritorySocialPerception::sense(const IAgent& self, const IWorld& world,
                                      std::span<float> out) const {
    if (out.size() < kDim) {
        return;
    }
    std::fill(out.begin(), out.begin() + kDim, 0.0f);

    const ProtagonistBrain* self_brain = asProtagonistBrain(self.brain());
    if (self_brain == nullptr) {
        return;
    }
    const std::span<const float> self_fp = self_brain->geneticFingerprint();

    // [0] inside own territory: my colony base footprint (BaseLayer) OR any
    // ground I've claimed with my own scent-marks (TerritoryMarkLayer). Marked
    // land lets the agent perceive territory it actively staked out, not just
    // the fixed colony.
    bool on_own_land = false;
    if (const auto* base = world.getLayer<BaseLayer>()) {
        on_own_land = base->inBase(self.position());
    }
    if (!on_own_land && !self_fp.empty()) {
        if (const auto* terr = world.getLayer<TerritoryMarkLayer>()) {
            on_own_land = terr->inOwnedTerritory(self.position(), self_fp);
        }
    }
    out[0] = on_own_land ? 1.0f : 0.0f;

    const auto* agent_layer = world.getLayer<AgentLayer>();
    if (agent_layer == nullptr) {
        return;
    }

    const auto* self_concrete = dynamic_cast<const Agent*>(&self);
    const bool self_in_cave = self_concrete != nullptr && self_concrete->isInCave();
    const float r2 = sense_radius_ * sense_radius_;
    const float self_energy = self.energy();

    // Pass over nearby agents: dominance (energy rank among co-species peers),
    // nearest ally (closest protagonist -> relatedness), nearest rival threat.
    std::size_t peer_count = 0;     // co-species protagonists in range (excl self)
    std::size_t weaker_count = 0;   // of those, how many I out-energy
    const Agent* nearest_ally = nullptr;
    float nearest_ally_d2 = r2;
    float max_rival_threat = 0.0f;

    for (const auto* other : agent_layer->agents()) {
        if (other == nullptr || !other->alive() || other->id() == self.id()) {
            continue;
        }
        if (other->isInCave() && !self_in_cave) {
            continue;
        }
        const float d2 = Vec2::distanceSquared(other->position(), self.position());
        if (d2 > r2) {
            continue;
        }
        const bool same_species = other->speciesId() == self.speciesId();
        const bool is_protagonist = asProtagonistBrain(other->brain()) != nullptr;

        if (is_protagonist && same_species) {
            ++peer_count;
            if (other->energy() < self_energy) {
                ++weaker_count;
            }
            if (d2 < nearest_ally_d2) {
                nearest_ally = other;
                nearest_ally_d2 = d2;
            }
        }

        // Rival/threat: predators always, plus non-kin agents. Kin (close
        // relatives) are not rivals.
        const float proximity = 1.0f - std::sqrt(d2) / sense_radius_;  // 1 near .. 0 edge
        const bool is_predator = other->isPredator();
        float relatedness = 0.0f;
        if (is_protagonist && !self_fp.empty()) {
            relatedness = relatednessFromFingerprints(self_fp,
                                                       asProtagonistBrain(other->brain())->geneticFingerprint());
        }
        constexpr float kAllyRelatedness = 0.6f;
        if (is_predator || relatedness < kAllyRelatedness) {
            float strength = self_energy + other->energy() > 1.0e-6f
                                 ? other->energy() / (self_energy + other->energy())
                                 : 0.5f;
            if (is_predator) {
                strength = std::max(strength, 0.85f);
            }
            max_rival_threat = std::max(max_rival_threat, proximity * strength);
        }
    }

    // [1] local dominance rank: fraction of nearby co-species peers I out-rank.
    // Alone (no peers) -> 1.0 (uncontested top of an empty hierarchy).
    out[1] = peer_count == 0 ? 1.0f
                             : static_cast<float>(weaker_count) / static_cast<float>(peer_count);

    // [2] nearest-ally relatedness/affinity.
    if (nearest_ally != nullptr && !self_fp.empty()) {
        out[2] = relatednessFromFingerprints(
            self_fp, asProtagonistBrain(nearest_ally->brain())->geneticFingerprint());
    }

    // [3] nearest-rival threat, plus standing territorial pressure: a foreign
    // clan's scent-marks covering my position are a threat even when no rival
    // agent is currently within sight.
    if (const auto* terr = world.getLayer<TerritoryMarkLayer>()) {
        max_rival_threat = std::max(max_rival_threat,
                                    terr->foreignStrengthAt(self.position(), self_fp));
    }
    out[3] = std::clamp(max_rival_threat, 0.0f, 1.0f);
}

}  // namespace neuro::routes::protagonist
