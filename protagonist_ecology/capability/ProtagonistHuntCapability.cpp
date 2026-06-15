#include "capability/ProtagonistHuntCapability.h"

#include "brain/ProtagonistBrain.h"
#include "core/agent/Agent.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"
#include "core/world/layers/object/ObjectLayer.h"

#include <cmath>

namespace neuro::routes::protagonist {

ProtagonistHuntCapability::ProtagonistHuntCapability(float hunt_radius,
                                                     float damage,
                                                     float energy_gain,
                                                     float cost,
                                                     bool enabled)
    : hunt_radius_(hunt_radius),
      damage_(damage),
      energy_gain_(energy_gain),
      cost_(cost),
      enabled_(enabled) {}

void ProtagonistHuntCapability::apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) {
    if (!enabled_ || outputs.empty() || outputs[0] < 0.3f) {
        return;
    }
    hunt_attempts.fetch_add(1, std::memory_order_relaxed);

    auto* concrete = dynamic_cast<Agent*>(&agent);
    if (concrete == nullptr) {
        return;
    }

    auto* agent_layer = world.getLayer<AgentLayer>();
    if (agent_layer == nullptr) {
        return;
    }

    concrete->setEnergy(concrete->energy() - cost_ * dt);

    const SimTimeSeconds now = world.currentTimeSeconds();

    // L2.7 v13 Detail #1+#6 (weapon bonus in melee): the protagonist's
    // carried object adds a damage multiplier. Spear is the strongest
    // (you forged it on purpose), stone is moderate, stick is a small
    // edge over bare hands. Without this, crafting a spear had zero
    // melee benefit (only thrown), which made the chop->base->craft
    // chain feel decorative.
    float weapon_mult = 1.0f;
    if (concrete->isCarrying()) {
        if (auto* objects = world.getLayer<ObjectLayer>(); objects != nullptr) {
            if (const auto* carried = objects->findById(concrete->carriedObject()); carried != nullptr) {
                switch (carried->type) {
                    case ObjectType::Spear: weapon_mult = 1.6f; break;
                    case ObjectType::Stone: weapon_mult = 1.3f; break;
                    case ObjectType::Stick: weapon_mult = 1.1f; break;
                    default: break;
                }
            }
        }
    }

    for (auto* other : agent_layer->agents()) {
        if (other == nullptr || other == &agent || !other->alive()) {
            continue;
        }

        auto* target = dynamic_cast<Agent*>(other);
        if (target == nullptr) {
            continue;
        }
        // Skip protagonist peers only (no friendly fire). Predators are
        // valid targets now: the protagonist brain decides when to
        // fight back vs flee using local social context provided by
        // AgentSocialPerception (peer_count / predator_count
        // dimensions). Energy gain on predator kills is also valid -
        // a slain wolf is meat too.
        if (dynamic_cast<const ProtagonistBrain*>(&target->brain()) != nullptr) {
            continue;
        }

        const float dx = agent.position().x - target->position().x;
        const float dy = agent.position().y - target->position().y;
        const float dist = std::sqrt(dx * dx + dy * dy);
        if (dist >= hunt_radius_) {
            continue;
        }

        const float actual_damage = damage_ * weapon_mult * dt;
        const float actual_gain = energy_gain_ * dt;

        // L2.1 v2 (SPECIES_DIFFERENTIATION_SPEC.md): damage threshold.
        // Same prey-side accumulator that PredatorHuntCapability uses;
        // protagonist hunting LargeHerbivore alone faces the same
        // wolfpack-style group requirement. Targets without threshold
        // (foragers, predators) take direct damage.
        constexpr SimTimeSeconds kThreatWindow = 1.0;
        // Energy is only gained when damage actually lands on the target.
        // Attacking a thresholded target without breaking its threshold
        // yields nothing - meat comes from wounds, not from swinging.
        bool damage_landed = false;
        if (target->damageThreshold() > 0.0f) {
            target->accumulateThreatDamage(agent.id(), actual_damage, now);
            const float window_total = target->threatDamageInWindow(now, kThreatWindow);
            if (window_total >= target->damageThreshold()) {
                target->setEnergy(target->energy() - window_total);
                target->clearThreatDamageWindow();
                damage_landed = true;
            }
        } else {
            target->setEnergy(target->energy() - actual_damage);
            damage_landed = true;
        }

        if (damage_landed) {
            concrete->setEnergy(concrete->energy() + actual_gain);
        }
        world.eventBus().publish(events::Event{
            events::EventType::AgentAttacked,
            events::AgentAttacked{agent.id(), target->id(), actual_damage},
            world.currentTick(),
            world.currentTimeSeconds()});
        concrete->noteDamageDealt(actual_damage);
        if (damage_landed && actual_gain > 0.0f) {
            concrete->noteMeatFood(actual_gain);
        }
        if (target->energy() <= 0.0f) {
            // L4 #3: prey hunted by protagonist → Predation cause.
            target->killWithCause(Agent::DeathCause::Predation);
            concrete->noteKill();
            hunt_successes.fetch_add(1, std::memory_order_relaxed);
            // L2.7 v13 HRL sub-reward: Hunt (GoalType=8) kill achieved.
            concrete->noteGoalCompletion(8);
        }
        break;
    }
}

}  // namespace neuro::routes::protagonist
