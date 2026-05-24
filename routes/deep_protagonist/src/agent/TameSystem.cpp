#include "agent/TameSystem.hpp"

#include <cmath>

namespace dp::agent {

TameRecord* TameSystem::find_or_create_(int animal_idx) {
    for (auto& r : records_) {
        if (r.animal_idx == animal_idx) return &r;
    }
    records_.push_back({animal_idx, 0.0f, false});
    return &records_.back();
}

TameSystem::FeedResult TameSystem::try_feed(
        const dp::world::AnimalSystem& animals,
        const Inventory& inv,
        float ax, float ay,
        float reach) {
    FeedResult result;
    float best_d2 = reach * reach;
    int   best_idx = -1;
    ItemKind chosen_food = ItemKind::Count;

    for (int i = 0; i < int(animals.animals().size()); ++i) {
        const auto& a = animals.animals()[i];
        if (a.state == dp::world::AnimalState::Dead) continue;
        ItemKind food = ItemKind::Count;
        if (a.kind == dp::world::AnimalKind::Rabbit
            || a.kind == dp::world::AnimalKind::Deer) {
            if (inv.num_of(ItemKind::Grass) > 0) food = ItemKind::Grass;
        } else if (a.kind == dp::world::AnimalKind::Wolf) {
            if (inv.num_of(ItemKind::Fur) > 0) food = ItemKind::Fur;
        } else {
            continue;  // fish/eagle out of scope
        }
        if (food == ItemKind::Count) continue;
        float dx = a.pos_x - ax;
        float dy = a.pos_y - ay;
        float d2 = dx*dx + dy*dy;
        if (d2 < best_d2) {
            best_d2 = d2;
            best_idx = i;
            chosen_food = food;
        }
    }

    if (best_idx < 0) return result;

    // Apply trust gain
    auto* rec = find_or_create_(best_idx);
    float gain = (chosen_food == ItemKind::Fur) ? 0.15f : 0.10f;
    bool was_following = rec->following;
    rec->trust += gain;
    if (rec->trust > 1.0f) rec->trust = 1.0f;
    if (rec->trust >= 0.5f) rec->following = true;

    result.animal_idx = best_idx;
    result.consumed   = chosen_food;
    result.just_became_follower = (!was_following && rec->following);
    return result;
}

void TameSystem::apply_follow(dp::world::AnimalSystem& animals,
                              float ax, float ay,
                              float dt,
                              float follow_speed) {
    for (const auto& r : records_) {
        if (!r.following) continue;
        // Tag the animal so wild wolves give it a pass, then steer it
        // toward the agent.
        animals.mark_tamed(r.animal_idx, true);
        animals.steer_toward(r.animal_idx, ax, ay, follow_speed, dt);
    }
}

int TameSystem::follower_count() const {
    int n = 0;
    for (const auto& r : records_) if (r.following) ++n;
    return n;
}

}  // namespace dp::agent
