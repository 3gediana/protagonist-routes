#pragma once

#include "core/types/Aliases.h"
#include "core/types/Vec2.h"

#include <cstddef>
#include <unordered_map>
#include <vector>

namespace neuro::routes::protagonist {

struct SocialRelationship {
    AgentId peer_id{0};
    Vec2 last_seen_pos{0.0f, 0.0f};
    Tick last_seen_tick{0};
    SimTimeSeconds last_seen_time_seconds{0.0};
    bool is_predator{false};
    float cooperation_score{0.0f};      // positive for joint build/hunt
    float communication_score{0.0f};    // positive when responding to signals
    float hostility_score{0.0f};        // positive if they attacked me
};

class SocialMemoryBank {
public:
    explicit SocialMemoryBank(std::size_t max_relationships);

    void reset();
    void tick(float dt);

    void recordSighting(AgentId peer_id, Vec2 pos, Tick tick, SimTimeSeconds time_seconds, bool is_predator);
    void rewardCooperation(AgentId peer_id, float amount);
    void rewardCommunication(AgentId peer_id, float amount);
    void recordHostility(AgentId peer_id, float damage);

    // Get relationships sorted by specific metrics
    std::vector<SocialRelationship> mostCooperative(std::size_t count) const;
    std::vector<SocialRelationship> mostHostile(std::size_t count) const;

    const std::unordered_map<AgentId, SocialRelationship>& relations() const { return relations_; }

private:
    std::size_t max_relations_;
    std::unordered_map<AgentId, SocialRelationship> relations_;
};

}  // namespace neuro::routes::protagonist
