#include "perception/KinPerception.h"

#include <algorithm>

namespace neuro::routes::protagonist {

void KinPerception::sense(const IAgent& /*self*/, const IWorld& /*world*/,
                          std::span<float> out) const {
    if (out.size() < kDim) {
        return;
    }
    // Phase D reserved channel: zero-filled until kinship logic lands.
    std::fill(out.begin(), out.begin() + kDim, 0.0f);
}

}  // namespace neuro::routes::protagonist
