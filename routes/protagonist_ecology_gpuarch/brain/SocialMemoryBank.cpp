#include "brain/SocialMemoryBank.h"

#include <algorithm>

namespace neuro::routes::protagonist {

SocialMemoryBank::SocialMemoryBank(std::size_t max_relationships)
    : max_relations_(max_relationships > 0 ? max_relationships : 32) {}

void SocialMemoryBank::reset() {
    relations_.clear();
}

void SocialMemoryBank::tick(float dt) {
    // Relationship values naturally decay or drift to neutrality over time
    for (auto& [_, rel] : relations_) {
        rel.cooperation_score = std::max(0.0f, rel.cooperation_score - 0.001f * dt);
        rel.communication_score = std::max(0.0f, rel.communication_score - 0.001f * dt);
        rel.hostility_score = std::max(0.0f, rel.hostility_score - 0.002f * dt); // Hostility forgotten slightly faster if not reinforced
    }
}

void SocialMemoryBank::recordSighting(AgentId peer_id,
                                      Vec2 pos,
                                      Tick tick,
                                      SimTimeSeconds time_seconds,
                                      bool is_predator) {
    auto it = relations_.find(peer_id);
    if (it != relations_.end()) {
        it->second.last_seen_pos = pos;
        it->second.last_seen_tick = tick;
        it->second.last_seen_time_seconds = time_seconds;
        it->second.is_predator = is_predator;
    } else {
        // Eviction logic if we exceed capacity
        if (relations_.size() >= max_relations_) {
            // Find oldest seen peer and evict
            auto oldest_it = relations_.begin();
            for (auto map_it = relations_.begin(); map_it != relations_.end(); ++map_it) {
                if (map_it->second.last_seen_time_seconds < oldest_it->second.last_seen_time_seconds) {
                    oldest_it = map_it;
                }
            }
            relations_.erase(oldest_it);
        }
        relations_.emplace(peer_id, SocialRelationship{peer_id, pos, tick, time_seconds, is_predator, 0.0f, 0.0f, 0.0f});
    }
}

void SocialMemoryBank::rewardCooperation(AgentId peer_id, float amount) {
    auto it = relations_.find(peer_id);
    if (it != relations_.end()) {
        it->second.cooperation_score = std::clamp(it->second.cooperation_score + amount, 0.0f, 1.0f);
    }
}

void SocialMemoryBank::rewardCommunication(AgentId peer_id, float amount) {
    auto it = relations_.find(peer_id);
    if (it != relations_.end()) {
        it->second.communication_score = std::clamp(it->second.communication_score + amount, 0.0f, 1.0f);
    }
}

void SocialMemoryBank::recordHostility(AgentId peer_id, float damage) {
    auto it = relations_.find(peer_id);
    if (it != relations_.end()) {
        it->second.hostility_score = std::clamp(it->second.hostility_score + damage * 0.05f, 0.0f, 1.0f);
    }
}

std::vector<SocialRelationship> SocialMemoryBank::mostCooperative(std::size_t count) const {
    std::vector<SocialRelationship> sorted;
    sorted.reserve(relations_.size());
    for (const auto& [_, rel] : relations_) {
        sorted.push_back(rel);
    }
    std::sort(sorted.begin(), sorted.end(), [](const SocialRelationship& a, const SocialRelationship& b) {
        return (a.cooperation_score + a.communication_score) > (b.cooperation_score + b.communication_score);
    });
    if (sorted.size() > count) {
        sorted.resize(count);
    }
    return sorted;
}

std::vector<SocialRelationship> SocialMemoryBank::mostHostile(std::size_t count) const {
    std::vector<SocialRelationship> sorted;
    sorted.reserve(relations_.size());
    for (const auto& [_, rel] : relations_) {
        sorted.push_back(rel);
    }
    std::sort(sorted.begin(), sorted.end(), [](const SocialRelationship& a, const SocialRelationship& b) {
        return a.hostility_score > b.hostility_score;
    });
    if (sorted.size() > count) {
        sorted.resize(count);
    }
    return sorted;
}

}  // namespace neuro::routes::protagonist
