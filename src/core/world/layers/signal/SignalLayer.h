#pragma once

#include "core/interfaces/IWorldLayer.h"
#include "core/types/Vec2.h"
#include "core/world/layers/signal/SignalEmitHistory.h"

#include <cstddef>
#include <vector>

namespace neuro {

class SignalLayer final : public IWorldLayer {
public:
    static constexpr const char* kName = "signal";
    static constexpr std::size_t kDefaultChannels = 4;

    SignalLayer(Vec2 world_size,
                std::size_t grid_resolution,
                std::size_t channels,
                float decay_rate,
                float diffusion_rate);

    const char* layerName() const override { return kName; }
    void onAttach(IWorld& world) override;
    void tick(IWorld& world, float dt) override;

    void emit(Vec2 position, const std::vector<float>& signal);
    void emitWithHistory(AgentId source, Vec2 position, const std::vector<float>& signal, Tick step_index, SimTimeSeconds time_seconds);
    std::vector<float> sample(Vec2 position, float radius) const;

    SignalEmitHistory& history() { return history_; }
    const SignalEmitHistory& history() const { return history_; }

    std::size_t channels() const { return channels_; }
    float decayRate() const { return decay_rate_; }
    double totalEnergy() const;

private:
    void applyDecay(float dt);
    void applyDiffusion();

    std::size_t gridIndex(float x, float y) const;
    Vec2 world_size_;
    std::size_t grid_res_;
    std::size_t channels_;
    float decay_rate_;
    float diffusion_rate_;
    std::vector<std::vector<float>> field_;
    SignalEmitHistory history_;
};

}  // namespace neuro
