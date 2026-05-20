#include "capability/ChopTreeCapability.h"

#include "core/agent/Agent.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "world/TreeLayer.h"

namespace neuro::routes::protagonist {

ChopTreeCapability::ChopTreeCapability(float chop_radius, float damage, float energy_cost, float activation_threshold)
    : chop_radius_(chop_radius),
      damage_(damage),
      energy_cost_(energy_cost),
      activation_threshold_(activation_threshold) {}

void ChopTreeCapability::apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) {
    if (outputs.empty() || outputs[0] < activation_threshold_) {
        return;
    }
    chop_attempts.fetch_add(1, std::memory_order_relaxed);

    auto* concrete = dynamic_cast<Agent*>(&agent);
    auto* trees = world.getLayer<TreeLayer>();
    if (concrete == nullptr || trees == nullptr || concrete->isCarrying()) {
        return;
    }

    if (!trees->chopNearest(agent.position(), chop_radius_, damage_ * dt, world)) {
        return;
    }

    concrete->setEnergy(concrete->energy() - energy_cost_ * dt);
    chop_successes.fetch_add(1, std::memory_order_relaxed);
}

}  // namespace neuro::routes::protagonist
