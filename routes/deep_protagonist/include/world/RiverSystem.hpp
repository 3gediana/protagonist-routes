#pragma once

#include "world/BiomeMap.hpp"
#include "world/Heightmap.hpp"

#include <cstdint>
#include <vector>

namespace dp::world {

// A simple river: a sequence of (x,y,z) waypoints carving a low-elevation
// path from a high-altitude spring to the world edge or to another river.
// Cells along the path are flattened in the heightmap and tagged as River
// in the BiomeMap. Adjacent cells become Swamp.
struct RiverSegment {
    float x0, y0, x1, y1;
};

class RiverSystem {
public:
    // Generate `count` rivers. Each river starts at a high random point and
    // descends following the steepest-descent path until it leaves the map
    // or reaches a depression. The heightmap is mutated in place to carve
    // the riverbed; the biome map is also updated.
    void generate(uint64_t seed, int count,
                  Heightmap& hm, BiomeMap& bm);

    const std::vector<RiverSegment>& segments() const { return segments_; }

private:
    std::vector<RiverSegment> segments_;
};

}  // namespace dp::world
