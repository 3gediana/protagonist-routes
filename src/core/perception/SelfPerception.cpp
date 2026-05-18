#include "core/perception/SelfPerception.h"

#include "core/interfaces/IAgent.h"

#include <algorithm>

namespace neuro {

SelfPerception::SelfPerception(float max_energy, Tick max_age_ticks)
    : max_energy_(max_energy),
      max_age_ticks_(max_age_ticks == 0 ? 1 : max_age_ticks) {}

std::size_t SelfPerception::dim() const {
    return 2;
}

void SelfPerception::sense(const IAgent& self, const IWorld&, std::span<float> out) const {
    out[0] = std::clamp(self.energy() / max_energy_, 0.0f, 1.0f);
    out[1] = std::clamp(static_cast<float>(self.age()) / static_cast<float>(max_age_ticks_), 0.0f, 1.0f);
}

const char* SelfPerception::name() const {
    return "self";
}

}  // namespace neuro
