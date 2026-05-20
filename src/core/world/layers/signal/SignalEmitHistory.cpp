#include "core/world/layers/signal/SignalEmitHistory.h"

#include <cmath>
#include <algorithm>

namespace neuro {

SignalEmitHistory::SignalEmitHistory(std::size_t capacity)
    : capacity_(capacity > 0 ? capacity : 1), records_(capacity_) {}

void SignalEmitHistory::record(AgentId source, std::size_t channel, Vec2 pos, Tick step_index, SimTimeSeconds time_seconds, float intensity) {
    records_[write_pos_] = EmitRecord{source, channel, pos, step_index, time_seconds, intensity};
    write_pos_ = (write_pos_ + 1) % capacity_;
    if (count_ < capacity_) ++count_;
}

std::vector<EmitRecord> SignalEmitHistory::query(Vec2 receiver_pos,
                                                  float radius,
                                                  SimTimeSeconds window_seconds,
                                                  SimTimeSeconds anchor_time_seconds) const {
    std::vector<EmitRecord> result;
    const float r2 = radius * radius;
    for (std::size_t i = 0; i < count_; ++i) {
        const std::size_t idx = (write_pos_ + capacity_ - count_ + i) % capacity_;
        const auto& rec = records_[idx];
        if (rec.time_seconds > anchor_time_seconds) continue;
        if (anchor_time_seconds - rec.time_seconds > window_seconds) continue;
        const float dx = rec.pos.x - receiver_pos.x;
        const float dy = rec.pos.y - receiver_pos.y;
        if (dx * dx + dy * dy <= r2) {
            result.push_back(rec);
        }
    }
    return result;
}

void SignalEmitHistory::clear() {
    write_pos_ = 0;
    count_ = 0;
}

}  // namespace neuro
