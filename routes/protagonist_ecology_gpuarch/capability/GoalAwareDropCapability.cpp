#include "capability/GoalAwareDropCapability.h"

#include "brain/GoalMemory.h"
#include "brain/ProtagonistBrain.h"
#include "core/agent/Agent.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/object/ObjectLayer.h"

namespace neuro::routes::protagonist {

namespace {

bool shouldHoldLogisticsMaterial(const Agent& agent, const MovableObject& object) {
    if (object.type != ObjectType::Stick && object.type != ObjectType::Stone) {
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

GoalAwareDropCapability::GoalAwareDropCapability(float energy_cost)
    : energy_cost_(energy_cost) {}

void GoalAwareDropCapability::apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) {
    (void)dt;
    if (outputs.empty() || outputs[0] < 0.5f) {
        return;
    }

    auto* concrete = dynamic_cast<Agent*>(&agent);
    if (concrete == nullptr || !concrete->isCarrying()) {
        return;
    }

    auto* obj_layer = world.getLayer<ObjectLayer>();
    if (obj_layer == nullptr) {
        return;
    }

    auto* obj = obj_layer->findById(concrete->carriedObject());
    if (obj == nullptr) {
        concrete->setCarriedObject(0);
        return;
    }
    if (shouldHoldLogisticsMaterial(*concrete, *obj)) {
        return;
    }

    obj->pos = agent.position();
    obj->velocity = agent.velocity() * 2.0f;
    obj->carried = false;
    concrete->setCarriedObject(0);
    concrete->setEnergy(concrete->energy() - energy_cost_);
    world.eventBus().publish(events::Event{events::EventType::ObjectDropped,
                                           events::ObjectDropped{agent.id(), obj->id},
                                           world.currentTick(),
                                           world.currentTimeSeconds()});
}

}  // namespace neuro::routes::protagonist
