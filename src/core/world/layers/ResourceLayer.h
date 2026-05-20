#pragma once

#include "core/interfaces/IWorldLayer.h"
#include "core/types/Aliases.h"
#include "core/types/Vec2.h"

#include <cstddef>
#include <optional>
#include <random>
#include <vector>

namespace neuro {

class ResourceLayer final : public IWorldLayer {
public:
    static constexpr const char* kName = "resource";

    struct Resource {
        Vec2 position;
        float amount;
        float max_amount;
        float regen_rate;
        bool available;
        SimTimeSeconds respawn_seconds_remaining;
        bool is_meat = false;       // false = plant, true = corpse meat
        bool is_transient = false;  // transient corpses won't respawn after consumption
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
    };

    ResourceLayer(std::size_t resource_count,
                  float resource_amount,
                  SimTimeSeconds respawn_seconds,
                  std::uint32_t seed,
                  bool dynamic = false,
                  float regen_rate = 0.0f,
                  bool terrain_adjustment_enabled = true);

    const char* layerName() const override;
    void onAttach(IWorld& world) override;
    void tick(IWorld& world, float dt) override;
    int tickOrder() const override;

    std::optional<NearestResource> nearestAvailable(Vec2 position) const;
    float tryConsume(Vec2 position, float radius, IWorld& world);
    ConsumeResult tryConsumeTyped(Vec2 position, float radius, IWorld& world);
    const std::vector<Resource>& resources() const;

    // Spawn a transient meat resource at the given position (used for corpses).
    // Decays naturally: not regenerated after consumption, removed after lifetime expires.
    void addCorpse(Vec2 position, float amount, IWorld& world);

private:
    Vec2 randomPosition(Vec2 world_size);
    void respawnResource(Resource& resource, IWorld& world);
    float terrainAdjustedAmount(Vec2 position, IWorld& world) const;

    std::size_t resource_count_;
    float resource_amount_;
    SimTimeSeconds respawn_seconds_;
    bool dynamic_;
    float regen_rate_;
    bool terrain_adjustment_enabled_;
    std::mt19937 rng_;
    Vec2 world_size_;
    std::vector<Resource> resources_;
};

}  // namespace neuro
