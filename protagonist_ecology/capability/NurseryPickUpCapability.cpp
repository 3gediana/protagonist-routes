#include "capability/NurseryPickUpCapability.h"

#include "core/agent/Agent.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"
#include "core/world/layers/object/ObjectLayer.h"
#include "world/BaseLayer.h"
#include "world/WorksiteLayer.h"

#include <cmath>

namespace neuro::routes::protagonist {

namespace {

bool canCoordinateHeavyPickup(const Agent* concrete, const MovableObject& object, const AgentLayer* agent_layer, float pick_radius) {
    if (!object.heavy || object.carriers_required <= 1 || agent_layer == nullptr) {
        return true;
    }

    std::size_t nearby_carriers = 0;
    for (const Agent* a : agent_layer->agents()) {
        if (a == concrete || !a->alive() || a->isCarrying()) {
            continue;
        }
        const float dx = a->position().x - object.pos.x;
        const float dy = a->position().y - object.pos.y;
        if (std::sqrt(dx * dx + dy * dy) < pick_radius * 1.5f) {
            ++nearby_carriers;
        }
    }

    return nearby_carriers >= object.carriers_required - 1;
}

}  // namespace

NurseryPickUpCapability::NurseryPickUpCapability(float pick_radius, float energy_cost)
    : pick_radius_(pick_radius), energy_cost_(energy_cost) {}

void NurseryPickUpCapability::apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) {
    (void)dt;
    if (outputs.empty() || outputs[0] < 0.5f) {
        return;
    }

    auto* concrete = dynamic_cast<Agent*>(&agent);
    if (concrete == nullptr || concrete->isCarrying()) {
        return;
    }

    auto* obj_layer = world.getLayer<ObjectLayer>();
    const auto* base = world.getLayer<BaseLayer>();
    const auto* worksite = world.getLayer<WorksiteLayer>();
    auto* agent_layer = world.getLayer<AgentLayer>();
    if (obj_layer == nullptr) {
        return;
    }

    const auto nearby = obj_layer->nearestObjects(agent.position(), pick_radius_);
    if (nearby.empty()) {
        return;
    }

    const bool allow_stockpile_pickup = worksite != nullptr && worksite->activeSiteCount() > 0;
    const MovableObject* selected = nullptr;
    for (const auto* candidate : nearby) {
        if (candidate == nullptr) {
            continue;
        }
        const bool in_stockpile = base != nullptr && base->inStockpile(candidate->pos);
        if (in_stockpile && !allow_stockpile_pickup) {
            continue;
        }
        if (candidate->heavy && canCoordinateHeavyPickup(concrete, *candidate, agent_layer, pick_radius_)) {
            selected = candidate;
            break;
        }
    }
    if (selected == nullptr) {
        for (const auto* candidate : nearby) {
            if (candidate == nullptr) {
                continue;
            }
            const bool in_stockpile = base != nullptr && base->inStockpile(candidate->pos);
            if (in_stockpile && !allow_stockpile_pickup) {
                continue;
            }
            selected = candidate;
            break;
        }
    }
    if (selected == nullptr) {
        return;
    }

    auto* obj = obj_layer->findById(selected->id);
    if (obj == nullptr) {
        return;
    }

    if (!canCoordinateHeavyPickup(concrete, *obj, agent_layer, pick_radius_)) {
        return;
    }

    obj->carried = true;
    concrete->setCarriedObject(obj->id);
    concrete->setEnergy(concrete->energy() - energy_cost_);
    // L2.7 v13 HRL sub-reward: GatherStick (GoalType=3) / GatherStone (4)
    if (obj->type == ObjectType::Stick) {
        concrete->noteGoalCompletion(3);
    } else if (obj->type == ObjectType::Stone) {
        concrete->noteGoalCompletion(4);
    }
    world.eventBus().publish(events::Event{events::EventType::ObjectPickedUp,
                                           events::ObjectPickedUp{agent.id(), obj->id},
                                           world.currentTick(),
                                           world.currentTimeSeconds()});
}

}  // namespace neuro::routes::protagonist
