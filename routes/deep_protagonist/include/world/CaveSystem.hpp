#pragma once

#include <cstdint>
#include <vector>

namespace dp::world {

// A simple cave system: a small number of axis-aligned box-shaped cavities
// carved into the terrain. The cavity volume sits BELOW the terrain surface;
// an agent inside the cavity is considered "underground", and the cave's
// ceiling acts as a normal collidable surface.
//
// This keeps S1 simple while still letting an agent meaningfully "enter" a
// cave. We can swap this for a proper voxel grid in S3 if richer topology
// is needed.
struct CaveBox {
    float center_x;   // world meters
    float center_y;
    float floor_z;    // world meters (lower bound)
    float ceiling_z;  // world meters (upper bound)
    float half_x;     // half-extent along x
    float half_y;     // half-extent along y

    bool contains(float x, float y, float z) const {
        return x >= center_x - half_x && x <= center_x + half_x
            && y >= center_y - half_y && y <= center_y + half_y
            && z >= floor_z         && z <= ceiling_z;
    }
};

class CaveSystem {
public:
    void add(const CaveBox& cave) { caves_.push_back(cave); }

    bool point_inside(float x, float y, float z) const {
        for (const auto& c : caves_) {
            if (c.contains(x, y, z)) return true;
        }
        return false;
    }

    const std::vector<CaveBox>& caves() const { return caves_; }

    // Procedurally place `count` caves under the terrain. Each cave is
    // anchored at a random horizontal location with a small entrance-shaft
    // that pokes out of the terrain so the agent can find it visually.
    void generate_random(uint64_t seed, int count,
                         float world_size_x, float world_size_y);

private:
    std::vector<CaveBox> caves_;
};

}  // namespace dp::world
