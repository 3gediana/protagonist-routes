#pragma once

#include "core/interfaces/IPerception.h"

#include <cstddef>
#include <vector>

namespace neuro {

class SignalPerception final : public IPerception {
public:
    static constexpr std::size_t kMaxChannels = 8;
    static constexpr std::size_t kFeaturesPerChannel = 2;

    static constexpr std::size_t featureDim(std::size_t channels) {
        return channels * kFeaturesPerChannel;
    }

    SignalPerception(std::size_t channels, float perception_radius, bool terrain_modulation_enabled = true);

    std::size_t dim() const override { return featureDim(channels_); }
    void sense(const IAgent& agent, const IWorld& world, std::span<float> output) const override;
    const char* name() const override { return "signal"; }

private:
    std::size_t channels_;
    float perception_radius_;
    bool terrain_modulation_enabled_;
    mutable std::vector<float> previous_sample_;
    mutable bool has_previous_sample_ = false;
};

}  // namespace neuro
