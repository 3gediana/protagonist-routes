#include "core/capability/PickUpCapability.h"

#include "core/agent/Agent.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"
#include "core/world/layers/object/ObjectLayer.h"

namespace neuro {

PickUpCapability::PickUpCapability(float pick_radius, float energy_cost)
    : pick_radius_(pick_radius), energy_cost_(energy_cost) {}

void PickUpCapability::apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) {
    if (outputs.empty() || outputs[0] < 0.5f) return;

    auto* concrete = dynamic_cast<Agent*>(&agent);
    if (concrete == nullptr || concrete->isCarrying()) return;

    auto* obj_layer = world.getLayer<ObjectLayer>();
    if (obj_layer == nullptr) return;

    auto nearby = obj_layer->nearestObjects(agent.position(), pick_radius_);
    if (nearby.empty()) return;

    // Pick up the nearest object
    auto* obj = const_cast<MovableObject*>(nearby[0]);

    // Heavy objects require multiple carriers nearby
    if (obj->heavy && obj->carriers_required > 1) {
        // Count how many non-carrying agents are near this object
        auto* agent_layer = world.getLayer<AgentLayer>();
        if (agent_layer) {
            std::size_t nearby_carriers = 0;
            for (const Agent* a : agent_layer->agents()) {
                if (a == concrete || !a->alive() || a->isCarrying()) continue;
                const float dx = a->position().x - obj->pos.x;
                const float dy = a->position().y - obj->pos.y;
                if (std::sqrt(dx * dx + dy * dy) < pick_radius_ * 1.5f) {
                    ++nearby_carriers;
                }
            }
            if (nearby_carriers < obj->carriers_required - 1) return;  // not enough helpers
        }
    }

    obj->carried = true;
    concrete->setCarriedObject(obj->id);
    concrete->setEnergy(concrete->energy() - energy_cost_);
    world.eventBus().publish(events::Event{events::EventType::ObjectPickedUp,
                                           events::ObjectPickedUp{agent.id(), obj->id},
                                           world.currentTick(),
                                           world.currentTimeSeconds()});
}

const char* PickUpCapability::name() const { return "pick_up"; }

}  // namespace neuro
