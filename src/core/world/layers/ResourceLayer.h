#pragma once

#include "core/interfaces/IWorldLayer.h"
#include "core/types/Aliases.h"
#include "core/types/Vec2.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <vector>

namespace neuro {

// L2.3 v2 (SPECIES_DIFFERENTIATION_SPEC.md § 3.1) - resource niche
// partitioning. Plant cells are spawned with one of three kinds at
// world setup; corpse cells are spawned by addCorpse() with kind =
// Meat. Agent eats are scaled by species-specific efficiency per
// kind, so e.g. LargeHerbivore gets full nutrition from Grass but
// almost nothing from Meat / Fruit.
enum class ResourceKind : std::uint8_t {
    Grass  = 0,  // low_quality_grass (abundant baseline)
    Browse = 1,  // high_quality_browse (rarer, terrain-tied)
    Fruit  = 2,  // fruit_seasonal (preferred by SmallForager / Omnivore)
    Meat   = 3,  // corpse / meat from kill
};

class ResourceLayer final : public IWorldLayer {
public:
    static constexpr const char* kName = "resource";

    // L2.3 v2: spawn distribution for plant cells. Defaults to all
    // grass for backward compatibility (matches old single-channel
    // ResourceLayer behaviour). Toml sets these to opt into multi-
    // channel niche partitioning.
    struct PlantSpawnWeights {
        float grass  = 1.0f;
        float browse = 0.0f;
        float fruit  = 0.0f;
    };

    struct Resource {
        Vec2 position;
        float amount;
        float max_amount;
        float regen_rate;
        bool available;
        SimTimeSeconds respawn_seconds_remaining;
        bool is_meat = false;       // false = plant, true = corpse meat (legacy mirror of kind == Meat)
        bool is_transient = false;  // transient corpses won't respawn after consumption
        ResourceKind kind = ResourceKind::Grass;
    };

    struct NearestResource {
        std::size_t index;
        Vec2 position;
        float amount;
        float distance;
    };

    struct ConsumeResult {
        float amount{0.0f};
        bool is_meat{false};
        ResourceKind kind{ResourceKind::Grass};
    };

    ResourceLayer(std::size_t resource_count,
                  float resource_amount,
                  SimTimeSeconds respawn_seconds,
                  std::uint32_t seed,
                  bool dynamic = false,
                  float regen_rate = 0.0f,
                  bool terrain_adjustment_enabled = true,
                  PlantSpawnWeights plant_weights = {});

    const char* layerName() const override;
    void onAttach(IWorld& world) override;
    void tick(IWorld& world, float dt) override;
    int tickOrder() const override;

    std::optional<NearestResource> nearestAvailable(Vec2 position) const;
    float tryConsume(Vec2 position, float radius, IWorld& world, float amount_limit = -1.0f);
    ConsumeResult tryConsumeTyped(Vec2 position, float radius, IWorld& world, float amount_limit = -1.0f);
    const std::vector<Resource>& resources() const;

    // Spawn a transient meat resource at the given position (used for corpses).
    // Decays naturally: not regenerated after consumption, removed after lifetime expires.
    void addCorpse(Vec2 position, float amount, IWorld& world);

private:
    Vec2 randomPosition(Vec2 world_size);
    void respawnResource(Resource& resource, IWorld& world);
    float terrainAdjustedAmount(Vec2 position, IWorld& world) const;

    ResourceKind sampleKind();

    std::size_t resource_count_;
    float resource_amount_;
    SimTimeSeconds respawn_seconds_;
    bool dynamic_;
    float regen_rate_;
    bool terrain_adjustment_enabled_;
    PlantSpawnWeights plant_weights_;
    std::mt19937 rng_;
    Vec2 world_size_;
    std::vector<Resource> resources_;
};

}  // namespace neuro
