#include "capability/ThrowCarriedObjectCapability.h"

#include "brain/ProtagonistBrain.h"
#include "core/agent/Agent.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"
#include "core/world/layers/object/ObjectLayer.h"

#include <cmath>

namespace neuro::routes::protagonist {

namespace {

bool shouldHoldStoneForLogistics(const Agent& agent, const MovableObject& object) {
    if (object.type != ObjectType::Stone) {
        return false;
    }

    const auto* brain = dynamic_cast<const ProtagonistBrain*>(&agent.brain());
    if (brain == nullptr) {
        return false;
    }

    switch (brain->goalMemory().currentGoal()) {
        case GoalType::GatherStick:
        case GoalType::GatherStone:
        case GoalType::BuildWorksite:
            return true;
        default:
            return false;
    }
}

}  // namespace

ThrowCarriedObjectCapability::ThrowCarriedObjectCapability(float throw_speed,
                                                           float throw_radius,
                                                           float stone_damage,
                                                           float spear_damage,
                                                           float energy_cost,
                                                           float stick_damage)
    : throw_speed_(throw_speed),
      throw_radius_(throw_radius),
      stone_damage_(stone_damage),
      spear_damage_(spear_damage),
      energy_cost_(energy_cost),
      stick_damage_(stick_damage) {}

void ThrowCarriedObjectCapability::apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) {
    if (outputs.size() < 2) {
        return;
    }

    const float mag_sq = outputs[0] * outputs[0] + outputs[1] * outputs[1];
    if (mag_sq < 0.25f) {
        return;
    }

    auto* concrete = dynamic_cast<Agent*>(&agent);
    auto* objects = world.getLayer<ObjectLayer>();
    auto* agents = world.getLayer<AgentLayer>();
    if (concrete == nullptr || objects == nullptr || agents == nullptr || !concrete->isCarrying()) {
        return;
    }

    auto* carried = objects->findById(concrete->carriedObject());
    if (carried == nullptr) {
        concrete->setCarriedObject(0);
        return;
    }
    if (shouldHoldStoneForLogistics(*concrete, *carried)) {
        return;
    }
    // L2.7 v13 Detail #4 (stick throw): allow Stick as a weak thrown
    // weapon (default 4 dmg vs stone's 10 / spear's 18). Without this
    // a protagonist holding only a stick had no offensive option;
    // sticks are abundant so this gives the network a cheap fallback.
    if (carried->type != ObjectType::Stone && carried->type != ObjectType::Spear && carried->type != ObjectType::Stick) {
        return;
    }
    throw_attempts.fetch_add(1, std::memory_order_relaxed);

    const float mag = std::sqrt(mag_sq);
    const Vec2 dir{outputs[0] / mag, outputs[1] / mag};
    float damage = stone_damage_;
    if (carried->type == ObjectType::Spear) damage = spear_damage_;
    else if (carried->type == ObjectType::Stick) damage = stick_damage_;

    Agent* best_target = nullptr;
    float best_dist = throw_radius_;
    for (auto* other : agents->agents()) {
        if (other == nullptr || other == concrete || !other->alive()) {
            continue;
        }
        const Vec2 delta = other->position() - concrete->position();
        const float dist = delta.length();
        if (dist <= 0.0001f || dist > throw_radius_) {
            continue;
        }
        const Vec2 delta_dir = delta / dist;
        const float alignment = delta_dir.x * dir.x + delta_dir.y * dir.y;
        if (alignment < 0.55f) {
            continue;
        }
        if (best_target == nullptr || dist < best_dist) {
            best_target = other;
            best_dist = dist;
        }
    }

    carried->carried = false;
    carried->velocity = agent.velocity() + dir * throw_speed_;
    carried->pos = best_target != nullptr ? best_target->position() : agent.position() + dir * 2.0f;
    concrete->setCarriedObject(0);
    concrete->setEnergy(concrete->energy() - energy_cost_ * dt);

    if (best_target != nullptr) {
        const float actual_damage = damage * dt;
        best_target->setEnergy(best_target->energy() - actual_damage);
        world.eventBus().publish(events::Event{
            events::EventType::AgentAttacked,
            events::AgentAttacked{agent.id(), best_target->id(), actual_damage},
            world.currentTick(),
            world.currentTimeSeconds()});
        concrete->noteDamageDealt(actual_damage);
        if (best_target->energy() <= 0.0f) {
            const char* cause = "thrown_stone";
            if (carried->type == ObjectType::Spear) cause = "thrown_spear";
            else if (carried->type == ObjectType::Stick) cause = "thrown_stick";
            best_target->kill(cause);
            concrete->noteKill();
            // L2.7 v13 HRL: thrown kill also counts as Hunt (GoalType=8)
            concrete->noteGoalCompletion(8);
        }
        throw_hits.fetch_add(1, std::memory_order_relaxed);
    }
}

}  // namespace neuro::routes::protagonist
