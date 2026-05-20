#include "core/reward/SignalBackpropReward.h"

#include "core/agent/Agent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"
#include "core/world/layers/signal/SignalEmitHistory.h"
#include "core/world/layers/signal/SignalLayer.h"

#include <cmath>
#include <unordered_map>

namespace neuro {

SignalBackpropReward::SignalBackpropReward(SignalEmitHistory& history,
                                           float backprop_factor,
                                           SimTimeSeconds backprop_window_seconds,
                                           float signal_perception_radius)
    : history_(history),
      backprop_factor_(backprop_factor),
      backprop_window_seconds_(backprop_window_seconds),
      signal_perception_radius_(signal_perception_radius) {}

void SignalBackpropReward::recordReceiverReward(AgentId receiver,
                                                Vec2 receiver_position,
                                                double reward_delta,
                                                SimTimeSeconds time_seconds) {
    if (reward_delta > 0.0) {
        reward_buffer_.push_back({receiver, receiver_position, reward_delta, time_seconds});
    }
}

void SignalBackpropReward::applyBackprop(IWorld& world, SimTimeSeconds current_time_seconds) {
    (void)current_time_seconds;
    auto* agent_layer = world.getLayer<AgentLayer>();
    if (agent_layer == nullptr) return;

    // Build lookup: agent_id -> Agent*
    std::unordered_map<std::uint64_t, Agent*> agent_lookup;
    for (auto* agent : agent_layer->agents()) {
        auto* concrete = dynamic_cast<Agent*>(agent);
        if (concrete) {
            agent_lookup[agent->id().value] = concrete;
        }
    }

    // For each positive reward event, find signal sources that the receiver
    // could have heard, and credit them
    std::unordered_map<std::uint64_t, double> source_credits;

    for (const auto& rec : reward_buffer_) {
        auto emit_records = history_.query(rec.position, signal_perception_radius_,
                                           backprop_window_seconds_, rec.time_seconds);

        double total_weight = 0.0;
        std::vector<double> emit_weights;
        emit_weights.reserve(emit_records.size());

        for (const auto& emit : emit_records) {
            const float dx = emit.pos.x - rec.position.x;
            const float dy = emit.pos.y - rec.position.y;
            const float distance = std::sqrt(dx * dx + dy * dy);
            const float distance_factor = std::max(0.0f, 1.0f - distance / std::max(signal_perception_radius_, 0.001f));
            const float recency_factor = 1.0f - static_cast<float>(rec.time_seconds - emit.time_seconds) /
                static_cast<float>(std::max<SimTimeSeconds>(backprop_window_seconds_, 1.0e-6));
            const double weight = static_cast<double>(std::max(emit.intensity, 0.0f)) *
                static_cast<double>(std::max(distance_factor, 0.0f)) *
                static_cast<double>(std::max(recency_factor, 0.0f));
            emit_weights.push_back(weight);
            total_weight += weight;
        }

        if (total_weight <= 0.0) {
            continue;
        }

        for (std::size_t i = 0; i < emit_records.size(); ++i) {
            if (emit_weights[i] <= 0.0) {
                continue;
            }
            source_credits[emit_records[i].source.value] +=
                rec.reward_delta * backprop_factor_ * (emit_weights[i] / total_weight);
        }
    }

    // Apply credits to source agents
    for (const auto& [source_id_val, credit] : source_credits) {
        auto it = agent_lookup.find(source_id_val);
        if (it != agent_lookup.end()) {
            it->second->addFitness(credit);
        }
    }

    reward_buffer_.clear();
}

void SignalBackpropReward::reset() {
    reward_buffer_.clear();
}

}  // namespace neuro
