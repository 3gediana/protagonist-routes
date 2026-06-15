#pragma once

#include "world/World.hpp"

#include <cstdint>
#include <vector>

namespace dp::world {

// Visual-only decorations for biomes: trees in forests, rocks on mountains,
// cacti in deserts. They have no collision in S2 - they are pure decoration
// to make the world feel alive. S3 will replace these with interactive
// vegetation/resource objects.
//
// Generation density is sparse (~one decoration every 4-8 cells) so we can
// keep total count under ~5000 for stable framerate.
enum class DecorationKind : uint8_t {
    Tree   = 0,
    Rock   = 1,
    Cactus = 2,
};

struct Decoration {
    float x;
    float y;
    float z;          // base height (terrain surface)
    float scale;      // multiplier on a per-kind base size
    float rotation;   // radians around Z, [0, 2pi)
    DecorationKind kind;
};

class Decorations {
public:
    void generate(uint64_t seed, const World& world);

    const std::vector<Decoration>& items() const { return items_; }

private:
    std::vector<Decoration> items_;
    uint64_t rng_state_ = 1;
    float frand();
};

}  // namespace dp::world
