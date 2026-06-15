#pragma once

#include "core/interfaces/IWorldLayer.h"
#include "core/types/Vec2.h"
#include "runtime/ProtagonistEcologyConfig.h"

#include <array>
#include <cstdint>
#include <random>
#include <unordered_map>

namespace neuro {
class Agent;
class AgentLayer;
class IWorld;
}

namespace neuro::routes::protagonist {

class MemoryTrialLayer final : public IWorldLayer {
public:
    static constexpr const char* kName = "protagonist_memory_trial";

    MemoryTrialLayer(const MemoryTrialConfig& config, std::uint32_t seed);

    const char* layerName() const override { return kName; }
    void onAttach(IWorld& world) override;
    void tick(IWorld& world, float dt) override;
    int tickOrder() const override { return 90; }

    std::array<float, 2> cueFor(Vec2 position) const;

    std::size_t correctChannel() const { return correct_channel_; }
    std::size_t successCount() const { return success_count_; }
    std::size_t failureCount() const { return failure_count_; }
    Vec2 cueAnchor() const { return cue_anchor_; }
    Vec2 hubAnchor() const { return hub_anchor_; }
    Vec2 leftTarget() const { return left_target_; }
    Vec2 rightTarget() const { return right_target_; }

private:
    struct AgentTrialState {
        bool sent_to_hub{false};
        bool resolved{false};
    };

    void placeAtCue(Agent& agent) const;
    void placeAtHub(Agent& agent) const;
    Vec2 stableOffset(std::uint64_t agent_value, float radius) const;

    MemoryTrialConfig config_;
    std::mt19937 rng_;
    AgentLayer* agents_{nullptr};
    Vec2 world_size_{};
    Vec2 cue_anchor_{};
    Vec2 hub_anchor_{};
    Vec2 left_target_{};
    Vec2 right_target_{};
    bool cue_active_{true};
    std::size_t correct_channel_{0};
    std::size_t success_count_{0};
    std::size_t failure_count_{0};
    std::unordered_map<std::uint64_t, AgentTrialState> agent_states_{};
};

}  // namespace neuro::routes::protagonist
