#include "capability/StealthCapability.h"

#include "core/agent/Agent.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"

#include <cmath>

namespace neuro::routes::protagonist {

void StealthCapability::apply(IAgent& agent,
                              IWorld& /*world*/,
                              std::span<const float> /*outputs*/,
                              float dt) {
    auto* concrete = dynamic_cast<Agent*>(&agent);
    if (concrete == nullptr) return;

    const auto v = concrete->velocity();
    const float speed = std::sqrt(v.x * v.x + v.y * v.y);
    if (speed >= velocity_epsilon_) {
        concrete->setStationarySeconds(0.0);
        if (concrete->isStealth()) {
            concrete->setStealth(false);
        }
        return;
    }

    concrete->setStationarySeconds(concrete->stationarySeconds() + dt);
    if (!concrete->isStealth()
        && concrete->stationarySeconds() >= stealth_threshold_seconds_) {
        concrete->setStealth(true);
    }
}

}  // namespace neuro::routes::protagonist
