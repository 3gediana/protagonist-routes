#pragma once

#include "core/types/Aliases.h"
#include "core/types/Vec2.h"

#include <cstddef>
#include <map>
#include <vector>

namespace neuro {

class SignalEmitHistory;
class IWorld;
class Agent;

class SignalBackpropReward {
public:
    SignalBackpropReward(SignalEmitHistory& history,
                         float backprop_factor,
                         std::size_t backprop_window,
                         float signal_perception_radius);

    void recordReceiverReward(AgentId receiver, Vec2 receiver_position, double reward_delta, std::size_t tick);

    void applyBackprop(IWorld& world, std::size_t current_tick);

    void reset();

private:
    SignalEmitHistory& history_;
    float backprop_factor_;
    std::size_t backprop_window_;
    float signal_perception_radius_;

    struct RewardRecord {
        AgentId receiver;
        Vec2 position;
        double reward_delta;
        std::size_t tick;
    };
    std::vector<RewardRecord> reward_buffer_;
};

}  // namespace neuro
