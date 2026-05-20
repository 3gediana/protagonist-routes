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
                         SimTimeSeconds backprop_window_seconds,
                         float signal_perception_radius);

    void recordReceiverReward(AgentId receiver, Vec2 receiver_position, double reward_delta, SimTimeSeconds time_seconds);

    void applyBackprop(IWorld& world, SimTimeSeconds current_time_seconds);

    void reset();

private:
    SignalEmitHistory& history_;
    float backprop_factor_;
    SimTimeSeconds backprop_window_seconds_;
    float signal_perception_radius_;

    struct RewardRecord {
        AgentId receiver;
        Vec2 position;
        double reward_delta;
        SimTimeSeconds time_seconds;
    };
    std::vector<RewardRecord> reward_buffer_;
};

}  // namespace neuro
