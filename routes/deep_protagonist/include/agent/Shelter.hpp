#pragma once

#include "agent/Inventory.hpp"

namespace dp::agent {

// One simple lean-to shelter the agent can build. Costs 1 Wood + 1 Grass
// (kept low so the agent can carry both materials in a single trip given
// the 3-slot inventory cap).
//
// While the agent stands within `radius` metres of the anchor we treat it
// as "sheltered": body temperature trends toward neutral and (in S3.7h)
// wolves give up sooner.
class Shelter {
public:
    static constexpr int kCostWood  = 1;
    static constexpr int kCostGrass = 1;

    bool placed() const { return placed_; }
    float x() const { return x_; }
    float y() const { return y_; }
    float z() const { return z_; }
    float radius() const { return radius_; }

    // Try to spend the inventory items and anchor a shelter at (x,y,z).
    // Returns true on success (items consumed). On failure, inventory is
    // untouched.
    bool try_place(Inventory& inv, float x, float y, float z);

    // True if (px, py) is within shelter radius of the anchor.
    bool covers(float px, float py) const;

    // Tear down (e.g. on episode reset).
    void clear() { placed_ = false; }

    // Tunable: how big a "covered area" the shelter creates.
    float radius_ = 3.0f;

private:
    bool  placed_ = false;
    float x_ = 0.0f, y_ = 0.0f, z_ = 0.0f;
};

}  // namespace dp::agent
