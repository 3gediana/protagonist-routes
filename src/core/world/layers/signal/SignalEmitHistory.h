#pragma once

#include "core/types/Vec2.h"
#include "core/types/Aliases.h"

#include <cstddef>
#include <vector>

namespace neuro {

struct EmitRecord {
    AgentId source;
    std::size_t channel;
    Vec2 pos;
    Tick step_index;
    SimTimeSeconds time_seconds;
    float intensity;
};

class SignalEmitHistory {
public:
    static constexpr std::size_t kDefaultCapacity = 2000;

    explicit SignalEmitHistory(std::size_t capacity = kDefaultCapacity);

    void record(AgentId source, std::size_t channel, Vec2 pos, Tick step_index, SimTimeSeconds time_seconds, float intensity);

    std::vector<EmitRecord> query(Vec2 receiver_pos,
                                   float radius,
                                   SimTimeSeconds window_seconds,
                                   SimTimeSeconds anchor_time_seconds) const;

    void clear();

private:
    std::size_t capacity_;
    std::vector<EmitRecord> records_;
    std::size_t write_pos_ = 0;
    std::size_t count_ = 0;
};

}  // namespace neuro
