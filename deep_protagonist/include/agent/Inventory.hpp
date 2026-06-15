#pragma once

#include "world/ResourceSystem.hpp"

#include <array>
#include <cstdint>

namespace dp::agent {

// What the agent can carry. Raw resources collected from the world plus
// crafted items. Capacity is intentionally tight (3 slots) so the agent
// has to make trade-offs and plan ahead.
enum class ItemKind : uint8_t {
    Wood       = 0,
    Stone      = 1,
    Grass      = 2,
    Spear      = 3,   // crafted: Wood + Stone
    Fur        = 4,   // raw material dropped by rabbit/deer/wolf kills
    GrassDress = 5,   // crafted: 3 Grass
    FurCloak   = 6,   // crafted: 2 Fur
    Seed       = 7,   // dropped when harvesting fruit/berry (S3.10c)
    Count      = 8,
};

const char* item_kind_name(ItemKind k);

inline ItemKind item_from_resource(dp::world::ResourceKind k) {
    switch (k) {
        case dp::world::ResourceKind::Wood:  return ItemKind::Wood;
        case dp::world::ResourceKind::Stone: return ItemKind::Stone;
        case dp::world::ResourceKind::Grass: return ItemKind::Grass;
    }
    return ItemKind::Wood;
}

// Fixed-capacity slot inventory.
class Inventory {
public:
    static constexpr int CAPACITY = 3;

    int  size()  const { return count_; }
    bool full()  const { return count_ >= CAPACITY; }
    bool empty() const { return count_ == 0; }

    ItemKind at(int i) const { return slots_[i]; }

    // Count how many of one kind are currently held.
    int num_of(ItemKind k) const;

    // Try to push one item; returns false if the inventory is full.
    bool add(ItemKind k);

    // Remove one matching item. Returns false if none present.
    bool remove_one(ItemKind k);

    // Crafting: consume 1 Wood + 1 Stone, produce 1 Spear. Returns true
    // on success (must have both items, and at least one slot freed up by
    // consumption is enough room for the Spear).
    bool craft_spear();

    // S3.10b crafting: 3 Grass -> 1 GrassDress (-2 net slots), or
    // 2 Fur -> 1 FurCloak (-1 net slot).
    bool craft_grass_dress();
    bool craft_fur_cloak();

    void clear();

private:
    std::array<ItemKind, CAPACITY> slots_{};
    int count_ = 0;
};

}  // namespace dp::agent
