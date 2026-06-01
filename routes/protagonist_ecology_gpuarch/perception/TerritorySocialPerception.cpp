#include "perception/TerritorySocialPerception.h"

#include <algorithm>

namespace neuro::routes::protagonist {

void TerritorySocialPerception::sense(const IAgent& /*self*/, const IWorld& /*world*/,
                                      std::span<float> out) const {
    if (out.size() < kDim) {
        return;
    }
    // Phase E reserved channel: zero-filled until social-structure logic lands.
    std::fill(out.begin(), out.begin() + kDim, 0.0f);
}

}  // namespace neuro::routes::protagonist
