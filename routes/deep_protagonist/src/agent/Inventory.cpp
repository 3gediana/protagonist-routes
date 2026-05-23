#include "agent/Inventory.hpp"

namespace dp::agent {

const char* item_kind_name(ItemKind k) {
    switch (k) {
        case ItemKind::Wood:  return "wood";
        case ItemKind::Stone: return "stone";
        case ItemKind::Grass: return "grass";
        case ItemKind::Spear: return "spear";
        default:              return "?";
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

void Inventory::clear() {
    count_ = 0;
}

}  // namespace dp::agent
