#include "capability/DrinkWaterCapability.h"

#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "world/SurvivalStatusLayer.h"
#include "world/WaterLayer.h"

namespace neuro::routes::protagonist {

DrinkWaterCapability::DrinkWaterCapability(float activation_threshold)
    : activation_threshold_(activation_threshold) {}

void DrinkWaterCapability::apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) {
    if (outputs.empty() || outputs[0] < activation_threshold_) {
        return;
    }

    auto* water = world.getLayer<WaterLayer>();
    auto* survival = world.getLayer<SurvivalStatusLayer>();
    if (water == nullptr || survival == nullptr) {
        return;
    }
    if (!water->inDrinkZone(agent.position())) {
        return;
    }

    survival->drink(agent.id(), water->drinkRate() * dt);
}

}  // namespace neuro::routes::protagonist
