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
    std::size_t tick;
    float intensity;
};

class SignalEmitHistory {
public:
    static constexpr std::size_t kDefaultCapacity = 2000;

    explicit SignalEmitHistory(std::size_t capacity = kDefaultCapacity);

    void record(AgentId source, std::size_t channel, Vec2 pos, std::size_t tick, float intensity);

    std::vector<EmitRecord> query(Vec2 receiver_pos,
                                   float radius,
                                   std::size_t window_ticks,
                                   std::size_t anchor_tick) const;

    void clear();

private:
    std::size_t capacity_;
    std::vector<EmitRecord> records_;
    std::size_t write_pos_ = 0;
    std::size_t count_ = 0;
};

}  // namespace neuro
