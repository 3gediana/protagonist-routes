#include "perception/BackgroundMemoryReadPerception.h"

#include "brain/DNCMemory.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "world/BackgroundMemoryLayer.h"

#include <algorithm>

namespace neuro::routes::protagonist {

void BackgroundMemoryReadPerception::sense(const IAgent& self,
                                           const IWorld& world,
                                           std::span<float> out) const {
    if (out.size() < read_vector_dim_) return;
    std::fill(out.begin(), out.begin() + read_vector_dim_, 0.0f);

    auto* layer = world.getLayer<BackgroundMemoryLayer>();
    if (layer == nullptr) return;
    const DNCMemory* mem = layer->find(self.id());
    if (mem == nullptr) return;

    const auto& v = mem->readVectors();
    const std::size_t n = std::min(read_vector_dim_, v.size());
    for (std::size_t i = 0; i < n; ++i) out[i] = v[i];
}

}  // namespace neuro::routes::protagonist
