#include "capability/IgniteFireCapability.h"

#include "core/agent/Agent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/object/ObjectLayer.h"
#include "world/CampfireLayer.h"
#include "world/NurseryLogisticsLayer.h"

#include <algorithm>

namespace neuro::routes::protagonist {

IgniteFireCapability::IgniteFireCapability(float ignite_radius,
                                           float fuel_per_stick,
                                           float energy_cost,
                                           float activation_threshold)
    : ignite_radius_(ignite_radius),
      fuel_per_stick_(fuel_per_stick),
      energy_cost_(energy_cost),
      activation_threshold_(activation_threshold) {}

void IgniteFireCapability::apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float /*dt*/) {
    if (outputs.empty() || outputs[0] < activation_threshold_) {
        return;
    }
    ignite_attempts.fetch_add(1, std::memory_order_relaxed);

    auto* concrete = dynamic_cast<Agent*>(&agent);
    auto* campfire = world.getLayer<CampfireLayer>();
    auto* objects = world.getLayer<ObjectLayer>();
    if (concrete == nullptr || campfire == nullptr || objects == nullptr) {
        return;
    }

    const Vec2 pos = concrete->position();

    // Firewood source: a loose stick within reach, or one from the stockpile.
    // The agent's *carried* tool (e.g. a spear-in-progress) is left alone.
    auto* logistics = world.getLayer<NurseryLogisticsLayer>();
    const bool have_stockpile_stick =
        (logistics != nullptr && logistics->stockpiledCount(ObjectType::Stick) > 0);

    auto find_loose_stick = [&]() -> MovableObject* {
        for (auto& object : objects->objects()) {
            if (object.carried || object.type != ObjectType::Stick) {
                continue;
            }
            if (Vec2::distanceSquared(object.pos, pos) > ignite_radius_ * ignite_radius_) {
                continue;
            }
            return &object;
        }
        return nullptr;
    };
    MovableObject* loose_stick = have_stockpile_stick ? nullptr : find_loose_stick();
    if (!have_stockpile_stick && loose_stick == nullptr) {
        return;  // no firewood within reach
    }

    // Must be next to a fire site. igniteOrRefuelNear returns false with no
    // side effects when none is in range, so we only spend the stick when a
    // fire was actually lit / refuelled.
    if (!campfire->igniteOrRefuelNear(pos, ignite_radius_, fuel_per_stick_)) {
        return;
    }

    // Spend exactly one stick of firewood.
    if (have_stockpile_stick) {
        logistics->consumeStockpiledObject(ObjectType::Stick);
    } else if (loose_stick != nullptr) {
        loose_stick->carried = true;
        loose_stick->velocity = kZeroVec2;
        loose_stick->pos = Vec2{-1024.0f, -1024.0f};
    }

    const float prev = concrete->energy();
    concrete->setEnergy(prev - energy_cost_);
    ignite_successes.fetch_add(1, std::memory_order_relaxed);
}

}  // namespace neuro::routes::protagonist
