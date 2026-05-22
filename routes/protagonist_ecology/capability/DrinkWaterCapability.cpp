#include "capability/DrinkWaterCapability.h"

#include "core/agent/Agent.h"
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
    // L2.7 v13 HRL sub-reward: FindWater / DrinkWater (GoalType=1)
    // also clear thirst drive directly here so the drive penalty (L2.2)
    // actually goes to 0 when the agent drinks. v10/v11 had drink_water
    // events stuck at 0 partly because drinking didn't feed back to drives.
    if (auto* concrete = dynamic_cast<Agent*>(&agent); concrete != nullptr) {
        concrete->noteGoalCompletion(1);
        concrete->driveState().thirst = 0.0f;
    }
}

}  // namespace neuro::routes::protagonist
