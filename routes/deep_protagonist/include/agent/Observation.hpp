#pragma once

#include "agent/AgentAction.hpp"
#include "agent/CapsuleAgent.hpp"
#include "agent/EpisodeManager.hpp"
#include "agent/Inventory.hpp"
#include "agent/ProgressTracker.hpp"
#include "agent/Shelter.hpp"
#include "agent/SpatialMemory.hpp"
#include "agent/TameSystem.hpp"
#include "agent/VitalSystem.hpp"
#include "agent/Wardrobe.hpp"
#include "world/AnimalSystem.hpp"
#include "world/BuildingSystem.hpp"
#include "world/Daylight.hpp"
#include "world/FarmingSystem.hpp"
#include "world/PlantSystem.hpp"
#include "world/ResourceSystem.hpp"
#include "world/World.hpp"

#include <array>

namespace dp::agent {

// PPO observation vector. Fixed size, normalised so each entry roughly
// lives in [-1, 1] (or [0, 1] for non-signed quantities). The order is
// stable so checkpoints stay portable across rebuilds.
//
// Layout (built by ObservationBuilder::compose). Original S3.10 base
// occupies slots 0..144 (145 dims). The S4.0 P0 extensions add 35 more
// slots so PPO can actually navigate, learn to find water, see what
// building it picked, etc. - see docs/BASELINE.md for the diagnosis.
//
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
//   124..126 progress  stage one-hot (early/mid/late)
//   127      collect_norm   resources_collected / mid_collect_threshold
//   128      explore_norm   total_explored / mid_explore_threshold
//   129      houses_built   houses_built / 5 (clipped)
//   130..132 building   nearest building dx, dy, completed (0/1)
//   133..136 inv_late   Fur / GrassDress / FurCloak / Seed counts / capacity
//   137..139 worn       one-hot (none / dress / cloak)
//   140..143 farm       nearest plot (dx, dy, growth, water_level)
//   144      tame       follower_count / 4 (clipped)
//   --- S4.0 P0 additions (35 slots) ---
//   145..147 nearest water (dx, dy, dist_norm) scanned in radius VISION_RANGE
//   148..153 biome one-hot at agent's feet (6 dims: Plain..Desert)
//   154..157 selected_building_type one-hot (4 dims: Shed/Wood/Stone/Big)
//   158..160 nearest active site materials left (wood/stone/grass each /20)
//   161..179 last_action: 3 continuous + 16 discrete (matches AgentAction)
//   --- S4.0 cognitive map (384 slots) ---
//   180..563 SpatialMemory 8x8 window x 6 features (row-major gy,gx,feat)
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
    static constexpr int N_BIOMES         = 6;
    static constexpr int N_BUILDING_TYPES = 4;

    static constexpr int SIZE =
          4                               // vital
        + 7                               // self
        + 1                               // episode
        + 2                               // day cycle
        + TERRAIN_PATCH * TERRAIN_PATCH   // terrain patch (25)
        + K_PLANTS    * 7                 // 28
        + K_ANIMALS   * 8                 // 32
        + K_RESOURCES * 6                 // 18
        + 4                               // inventory counts
        + 3                               // shelter (placed, dx, dy)
        + 3                               // progress stage one-hot
        + 1 + 1 + 1                       // collect/explore/houses norms
        + 3                               // nearest building
        + 4                               // inv_late
        + 3                               // worn one-hot
        + 4                               // nearest farm
        + 1                               // tame
        // S4.0 P0 additions:
        + 3                               // nearest_water (dx,dy,dist)
        + N_BIOMES                        // biome one-hot (6)
        + N_BUILDING_TYPES                // selected_building_type one-hot (4)
        + 3                               // nearest blueprint materials left
        + ACTION_CONTINUOUS_DIM           // last_action continuous (3)
        + ACTION_DISCRETE_DIM             // last_action discrete   (16)
        // S4.0 cognitive map (read-out of SpatialMemory window):
        + SpatialMemory::WINDOW_DIM;      // 8 * 8 * 6 = 384

    using Vector = std::array<float, SIZE>;

    // Compose one observation snapshot. Output is filled in place.
    // selected_building_type is the BuildingType the agent will place
    // next time it triggers place_blueprint (cycled by Tab). It's needed
    // here because the value lives outside the systems we already pass.
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
                        const ProgressTracker&              progress,
                        const dp::world::BuildingSystem&    buildings,
                        const Wardrobe&                     wardrobe,
                        const dp::world::FarmingSystem&     farming,
                        const TameSystem&                   tame,
                        dp::world::BuildingType             selected_building_type,
                        const SpatialMemory&                spatial,
                        Vector&                             out);
};

}  // namespace dp::agent
