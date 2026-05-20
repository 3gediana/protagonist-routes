#pragma once

#include "core/types/Aliases.h"
#include "core/types/Vec2.h"

#include <cstddef>
#include <string>
#include <vector>

namespace neuro::routes::protagonist {

struct EpisodicEvent {
    Tick tick{0};
    SimTimeSeconds time_seconds{0.0};
    Vec2 pos{0.0f, 0.0f};
    std::string type;
    float salience{1.0f};  // Importance of event
};

class EpisodicMemoryBank {
public:
    explicit EpisodicMemoryBank(std::size_t max_events);

    void reset();
    void recordEvent(Tick tick, SimTimeSeconds time_seconds, Vec2 pos, const std::string& type, float salience);

    // Get top salient events
    std::vector<EpisodicEvent> topSalient(std::size_t count) const;
    
    // Get recent events of specific type
    std::vector<EpisodicEvent> recentOfType(const std::string& type, std::size_t count) const;

    const std::vector<EpisodicEvent>& events() const { return events_; }
    std::size_t maxEvents() const { return max_events_; }

private:
    std::size_t max_events_;
    std::vector<EpisodicEvent> events_;
};

}  // namespace neuro::routes::protagonist
