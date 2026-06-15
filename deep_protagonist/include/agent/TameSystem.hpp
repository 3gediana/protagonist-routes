#pragma once

#include "agent/Inventory.hpp"
#include "world/AnimalSystem.hpp"

#include <vector>

namespace dp::agent {

struct TameRecord {
    int   animal_idx;       // index into AnimalSystem::animals()
    float trust       = 0.0f;  // [0,1], threshold 0.5 to start following
    bool  following   = false;
};

// Light taming system: feeding animals raises a "trust" stat. Once trust
// crosses 0.5 the animal becomes a follower and trails the agent on
// every tick.
//
// Feed rules:
//   Rabbit / Deer accept Grass    -> +0.10 trust per feed
//   Wolf accepts Fur (proxy for meat) -> +0.15 trust per feed
//   Fish / Eagle ignored (out of scope)
//
// We intentionally keep this small: the agent should learn to *spend*
// hard-won resources on follower acquisition, which is itself a learned
// behaviour we want to surface.
class TameSystem {
public:
    // Try to feed the closest matching animal within `reach` metres.
    // Returns the animal index that was fed (-1 if no match), and the
    // ItemKind consumed - the caller is responsible for removing it from
    // the inventory.
    struct FeedResult {
        int      animal_idx = -1;
        ItemKind consumed   = ItemKind::Count;
        bool     just_became_follower = false;
    };
    FeedResult try_feed(const dp::world::AnimalSystem& animals,
                        const Inventory& inv,
                        float ax, float ay,
                        float reach = 3.0f);

    // Apply follow steering to all followers. Call once per tick.
    void apply_follow(dp::world::AnimalSystem& animals,
                      float ax, float ay,
                      float dt,
                      float follow_speed = 4.0f);

    const std::vector<TameRecord>& records() const { return records_; }
    int follower_count() const;

    // Episode reset.
    void clear() { records_.clear(); }

private:
    std::vector<TameRecord> records_;

    TameRecord* find_or_create_(int animal_idx);
};

}  // namespace dp::agent
