#include "core/capability/DropCapability.h"

#include "core/agent/Agent.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/object/ObjectLayer.h"

namespace neuro {

DropCapability::DropCapability(float energy_cost)
    : energy_cost_(energy_cost) {}

void DropCapability::apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) {
    if (outputs.empty() || outputs[0] < 0.5f) return;

    auto* concrete = dynamic_cast<Agent*>(&agent);
    if (concrete == nullptr || !concrete->isCarrying()) return;

    auto* obj_layer = world.getLayer<ObjectLayer>();
    if (obj_layer == nullptr) return;

    auto* obj = obj_layer->findById(concrete->carriedObject());
    if (obj == nullptr) {
        concrete->setCarriedObject(0);
        return;
    }

    // Drop at current position with agent velocity as initial push
    obj->pos = agent.position();
    obj->velocity = agent.velocity() * 2.0f;  // push with 2x agent speed
    obj->carried = false;
    concrete->setCarriedObject(0);
    concrete->setEnergy(concrete->energy() - energy_cost_);
    world.eventBus().publish(events::Event{events::EventType::ObjectDropped,
                                           events::ObjectDropped{agent.id(), obj->id},
                                           world.currentTick()});
}

const char* DropCapability::name() const { return "drop"; }

}  // namespace neuro
