#include "agent/Inventory.hpp"

namespace dp::agent {

const char* item_kind_name(ItemKind k) {
    switch (k) {
        case ItemKind::Wood:       return "wood";
        case ItemKind::Stone:      return "stone";
        case ItemKind::Grass:      return "grass";
        case ItemKind::Spear:      return "spear";
        case ItemKind::Fur:        return "fur";
        case ItemKind::GrassDress: return "grass_dress";
        case ItemKind::FurCloak:   return "fur_cloak";
        case ItemKind::Seed:       return "seed";
        default:                   return "?";
    }
}

int Inventory::num_of(ItemKind k) const {
    int n = 0;
    for (int i = 0; i < count_; ++i) {
        if (slots_[i] == k) ++n;
    }
    return n;
}

bool Inventory::add(ItemKind k) {
    if (full()) return false;
    slots_[count_++] = k;
    return true;
}

bool Inventory::remove_one(ItemKind k) {
    for (int i = 0; i < count_; ++i) {
        if (slots_[i] == k) {
            // Shift left to keep slots packed
            for (int j = i; j < count_ - 1; ++j) slots_[j] = slots_[j + 1];
            --count_;
            return true;
        }
    }
    return false;
}

bool Inventory::craft_spear() {
    if (num_of(ItemKind::Wood) < 1) return false;
    if (num_of(ItemKind::Stone) < 1) return false;
    remove_one(ItemKind::Wood);
    remove_one(ItemKind::Stone);
    // Net: -2 slots, +1 slot, so we always have room.
    add(ItemKind::Spear);
    return true;
}

bool Inventory::craft_grass_dress() {
    // D-112 bugfix (decision_round_10): a grass dress costs 1 grass, not 3.
    // CAPACITY==3 and Environment.cpp:253 pre-places 1 grass at spawn "so first
    // craft_grass_dress works step 0" -- a 3-grass cost made that intent
    // logically impossible (the agent can never free all 3 slots for grass
    // while it also carries wood/stone), so clothing was never craftable. The
    // dress resist is only 0.10, so this cannot game the survival metric; it
    // restores the documented step-0 craftability.
    if (num_of(ItemKind::Grass) < 1) return false;
    remove_one(ItemKind::Grass);
    add(ItemKind::GrassDress);
    return true;
}

bool Inventory::craft_fur_cloak() {
    if (num_of(ItemKind::Fur) < 2) return false;
    for (int i = 0; i < 2; ++i) remove_one(ItemKind::Fur);
    add(ItemKind::FurCloak);
    return true;
}

void Inventory::clear() {
    count_ = 0;
}

}  // namespace dp::agent
