#pragma once

#include "core/interfaces/IPerception.h"

#include <cstddef>

namespace neuro {

class SignalPerception final : public IPerception {
public:
    static constexpr std::size_t kMaxChannels = 8;

    SignalPerception(std::size_t channels, float perception_radius);

    std::size_t dim() const override { return channels_; }
    void sense(const IAgent& agent, const IWorld& world, std::span<float> output) const override;
    const char* name() const override { return "signal"; }

private:
    std::size_t channels_;
    float perception_radius_;
};

}  // namespace neuro
