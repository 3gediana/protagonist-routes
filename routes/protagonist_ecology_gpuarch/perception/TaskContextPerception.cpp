#include "TaskContextPerception.h"

#include "core/agent/Agent.h"

#include <algorithm>

namespace neuro::routes::protagonist {

void TaskContextPerception::sense(const IAgent& self, const IWorld& world, std::span<float> out) const {
    (void)world;
    std::fill(out.begin(), out.end(), 0.0f);
    const auto* concrete = dynamic_cast<const Agent*>(&self);
    if (concrete == nullptr) return;
    const int idx = concrete->taskTemplateIndex();
    if (idx >= 0 && static_cast<std::size_t>(idx) < kDim) {
        out[static_cast<std::size_t>(idx)] = 1.0f;
    }
}

}  // namespace neuro::routes::protagonist
