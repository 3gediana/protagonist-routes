#include "agent/Wardrobe.hpp"

namespace dp::agent {

bool Wardrobe::wear(Inventory& inv, ItemKind k) {
    if (k != ItemKind::GrassDress && k != ItemKind::FurCloak) return false;
    if (inv.num_of(k) < 1) return false;
    inv.remove_one(k);
    // If we already had something on, drop it back into inventory if we
    // have room - otherwise silently abandon it.
    if (worn_ != ItemKind::Count) {
        if (!inv.full()) inv.add(worn_);
    }
    worn_ = k;
    return true;
}

void Wardrobe::take_off() {
    worn_ = ItemKind::Count;
}

float Wardrobe::cold_resist() const {
    switch (worn_) {
        case ItemKind::GrassDress: return 0.10f;
        case ItemKind::FurCloak:   return 0.30f;
        default:                    return 0.0f;
    }
}

void Wardrobe::clear() {
    worn_ = ItemKind::Count;
}

}  // namespace dp::agent
