#pragma once

#include "agent/CapsuleAgent.hpp"
#include "agent/EpisodeManager.hpp"
#include "agent/Inventory.hpp"
#include "agent/Shelter.hpp"
#include "agent/VitalSystem.hpp"
#include "world/AnimalSystem.hpp"
#include "world/Daylight.hpp"
#include "world/PlantSystem.hpp"
#include "world/ResourceSystem.hpp"
#include "world/World.hpp"

#include <array>

namespace dp::agent {

// PPO observation vector. Fixed size, normalised so each entry roughly
// lives in [-1, 1] (or [0, 1] for non-signed quantities). The order is
// stable so checkpoints stay portable across rebuilds.
//
// Layout (built by ObservationBuilder::compose):
//   0..3    vital     hunger / thirst / stamina / temperature (each /100)
//   4..10   self      vel_x/10, vel_y/10, vel_z/10, sin(yaw), cos(yaw),
//                     inside_cave (0/1), on_ground (0/1)
//   11      episode   elapsed_s / max_seconds (clipped to [0,1])
//   12..13  day       sin(2*pi*t), cos(2*pi*t)
//   14..38  terrain   5x5 patch of (height - my_z) / 30m at +-10m offsets
//   39..66  plants    4 nearest ripe plants: (dx, dy, dz, one-hot kind 4)
//   67..98  animals   4 nearest live animals: (dx, dy, dz, one-hot kind 5)
//   99..116 resources 3 nearest available resources: (dx, dy, dz, one-hot 3)
//   117..120 inventory  W/S/G/Spear counts each / capacity
//   121..123 shelter    placed (0/1), dx, dy from agent (clipped to vision)
//
// All distances are normalised by VISION_RANGE (60m) so |dx|,|dy|,|dz| <= 1.
class ObservationBuilder {
public:
    static constexpr int K_PLANTS         = 4;
    static constexpr int K_ANIMALS        = 4;
    static constexpr int K_RESOURCES      = 3;
    static constexpr int TERRAIN_PATCH    = 5;
    static constexpr float VISION_RANGE   = 60.0f;
    static constexpr float TERRAIN_STRIDE = 10.0f;  // metres between patch samples
    static constexpr float MAX_VEL        = 10.0f;
    static constexpr float MAX_HEIGHT_REL = 30.0f;

    static constexpr int SIZE =
          4                               // vital
        + 7                               // self
        + 1                               // episode
        + 2                               // day cycle
        + TERRAIN_PATCH * TERRAIN_PATCH   // terrain patch
        + K_PLANTS    * 7                 // 4 plants * (dx,dy,dz,kind_4)
        + K_ANIMALS   * 8                 // 4 animals * (dx,dy,dz,kind_5)
        + K_RESOURCES * 6                 // 3 resources * (dx,dy,dz,kind_3)
        + 4                               // inventory counts
        + 3;                              // shelter (placed, dx, dy)

    using Vector = std::array<float, SIZE>;

    // Compose one observation snapshot. Output is filled in place.
    static void compose(const dp::world::World&            world,
                        const CapsuleAgent&                 agent,
                        const VitalState&                   vital_state,
                        const dp::world::Daylight&          day,
                        const dp::world::PlantSystem&       plants,
                        const dp::world::AnimalSystem&      animals,
                        const dp::world::ResourceSystem&    resources,
                        const Inventory&                    inventory,
                        const Shelter&                      shelter,
                        const EpisodeManager&               episodes,
                        Vector&                             out);
};

}  // namespace dp::agent
