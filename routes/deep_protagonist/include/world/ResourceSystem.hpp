#pragma once

#include "world/World.hpp"

#include <cstdint>
#include <vector>

namespace dp::world {

// Three raw resources the agent will need for the toolchain
// (blueprint: stick + sharp stone -> spear; sticks + grass -> shelter):
//
//   Wood   - dry sticks scattered in / near forests
//   Stone  - chips on mountain biomes and rocky decorations
//   Grass  - long blades in plains and swamps (used to lash shelters)
//
// Each instance is a discrete pickup. After collection the spot enters a
// regrow timer (so the world doesn't bleed dry over an episode).
enum class ResourceKind : uint8_t {
    Wood  = 0,
    Stone = 1,
    Grass = 2,
};

const char* resource_kind_name(ResourceKind k);

struct ResourceInstance {
    float        x, y, z;
    ResourceKind kind;
    bool         available    = true;
    float        regrow_timer = 0.0f;
};

class ResourceSystem {
public:
    // Lay out resources across the world based on biomes.
    void initialize(uint64_t seed, const World& world);

    // Advance regrow timers; collected nodes that have ticked past their
    // cooldown become available again.
    void tick(float dt);

    // Pick up any available resource within `radius` of (x,y,z). Returns
    // the KIND list collected this call. Multiple may be returned if the
    // agent is standing on a cluster.
    std::vector<ResourceKind> try_collect(float x, float y, float z,
                                          float radius = 1.5f);

    const std::vector<ResourceInstance>& resources() const { return resources_; }

    // How many of each kind are currently available (for HUD / observation).
    int available_count(ResourceKind k) const;

    // Tunables (seconds before a collected node respawns)
    float wood_regrow_seconds  = 90.0f;
    float stone_regrow_seconds = 180.0f;
    float grass_regrow_seconds = 30.0f;

    // Initial counts per kind (scaled inside initialize() to world size).
    int wood_target_count  = 600;
    int stone_target_count = 400;
    int grass_target_count = 800;

private:
    std::vector<ResourceInstance> resources_;
    uint64_t rng_state_ = 1;

    float frand();
    float regrow_for(ResourceKind k) const;
    void  scatter(const World& w, ResourceKind kind, int target_count,
                  bool (*biome_filter)(Biome));
};

}  // namespace dp::world
