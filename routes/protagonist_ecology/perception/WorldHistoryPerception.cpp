#include "perception/WorldHistoryPerception.h"

#include <algorithm>

namespace neuro::routes::protagonist {

void WorldHistoryPerception::sense(const IAgent& /*self*/, const IWorld& /*world*/,
                                   std::span<float> out) const {
    if (out.size() < kDim) {
        return;
    }
    // Phase F reserved channel: zero-filled until world-history logic lands.
    std::fill(out.begin(), out.begin() + kDim, 0.0f);
}

}  // namespace neuro::routes::protagonist
