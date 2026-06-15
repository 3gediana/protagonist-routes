#include "brain/EpisodicMemoryBank.h"

#include <algorithm>

namespace neuro::routes::protagonist {

EpisodicMemoryBank::EpisodicMemoryBank(std::size_t max_events)
    : max_events_(max_events > 0 ? max_events : 32) {
    events_.reserve(max_events_);
}

void EpisodicMemoryBank::reset() {
    events_.clear();
}

void EpisodicMemoryBank::recordEvent(Tick tick,
                                     SimTimeSeconds time_seconds,
                                     Vec2 pos,
                                     const std::string& type,
                                     float salience) {
    // If memory is full, we evict the least salient event, unless the new event is even less salient.
    if (events_.size() >= max_events_) {
        auto least_salient_it = std::min_element(events_.begin(), events_.end(),
            [](const EpisodicEvent& a, const EpisodicEvent& b) {
                return a.salience < b.salience;
            });
        if (least_salient_it != events_.end() && salience > least_salient_it->salience) {
            *least_salient_it = EpisodicEvent{tick, time_seconds, pos, type, salience};
        }
    } else {
        events_.push_back(EpisodicEvent{tick, time_seconds, pos, type, salience});
    }
}

std::vector<EpisodicEvent> EpisodicMemoryBank::topSalient(std::size_t count) const {
    std::vector<EpisodicEvent> sorted = events_;
    std::sort(sorted.begin(), sorted.end(), [](const EpisodicEvent& a, const EpisodicEvent& b) {
        return a.salience > b.salience; // Highest salience first
    });
    if (sorted.size() > count) {
        sorted.resize(count);
    }
    return sorted;
}

std::vector<EpisodicEvent> EpisodicMemoryBank::recentOfType(const std::string& type, std::size_t count) const {
    std::vector<EpisodicEvent> matches;
    matches.reserve(events_.size());
    for (const auto& evt : events_) {
        if (evt.type == type) {
            matches.push_back(evt);
        }
    }
    // Sort chronological descending (newest first)
    std::sort(matches.begin(), matches.end(), [](const EpisodicEvent& a, const EpisodicEvent& b) {
        return a.time_seconds > b.time_seconds;
    });
    if (matches.size() > count) {
        matches.resize(count);
    }
    return matches;
}

}  // namespace neuro::routes::protagonist
