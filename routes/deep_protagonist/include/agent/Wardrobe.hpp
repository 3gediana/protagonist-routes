#pragma once

#include "agent/Inventory.hpp"

namespace dp::agent {

// Light clothing slot. Holds at most one worn item; wearing is mutually
// exclusive, but a stockpile can sit in Inventory waiting to be donned.
//
// Cold resistance scales how slowly cold biomes / nights drain body
// temperature. 0 = no protection, 1 = full immunity (we cap at ~0.3 in
// practice).
class Wardrobe {
public:
    ItemKind worn() const { return worn_; }
    bool wearing(ItemKind k) const { return worn_ == k; }

    // Wear an item from the inventory. Returns false if the item isn't
    // present or isn't a clothing kind. Replacing an existing piece is
    // allowed - the old garment goes back into inventory if there's
    // room, otherwise it's silently discarded.
    bool wear(Inventory& inv, ItemKind k);

    void take_off();

    // Cold-drain damping factor: 0 = full drain, 1 = no drain at all.
    // (Used as a multiplier on the temperature loss rate.)
    float cold_resist() const;

    void clear();

private:
    ItemKind worn_ = ItemKind::Count;  // "no clothing" sentinel
};

}  // namespace dp::agent
