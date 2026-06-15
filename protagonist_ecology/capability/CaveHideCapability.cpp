#include "capability/CaveHideCapability.h"

#include "core/agent/Agent.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/terrain/TerrainLayer.h"

namespace neuro::routes::protagonist {

void CaveHideCapability::apply(IAgent& agent,
                               IWorld& world,
                               std::span<const float> /*outputs*/,
                               float /*dt*/) {
    auto* concrete = dynamic_cast<Agent*>(&agent);
    if (concrete == nullptr) return;

    const auto* terrain = world.getLayer<TerrainLayer>();
    if (terrain == nullptr) {
        // No terrain layer in this world (smoke / unit tests) - leave
        // the flag in whatever state external code (or a test) set.
        return;
    }

    const auto pos = agent.position();
    concrete->setInCave(terrain->inCave(pos.x, pos.y));
}

}  // namespace neuro::routes::protagonist
