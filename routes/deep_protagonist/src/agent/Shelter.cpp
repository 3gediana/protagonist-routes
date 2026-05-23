#include "agent/Shelter.hpp"

namespace dp::agent {

bool Shelter::try_place(Inventory& inv, float x, float y, float z) {
    // Must have 1 wood + 1 grass on hand.
    if (inv.num_of(ItemKind::Wood)  < kCostWood)  return false;
    if (inv.num_of(ItemKind::Grass) < kCostGrass) return false;

    // Spend resources.
    for (int i = 0; i < kCostWood;  ++i) inv.remove_one(ItemKind::Wood);
    for (int i = 0; i < kCostGrass; ++i) inv.remove_one(ItemKind::Grass);

    // Anchor
    placed_ = true;
    x_ = x;
    y_ = y;
    z_ = z;
    return true;
}

bool Shelter::covers(float px, float py) const {
    if (!placed_) return false;
    float dx = px - x_;
    float dy = py - y_;
    return (dx*dx + dy*dy) <= (radius_ * radius_);
}

}  // namespace dp::agent
