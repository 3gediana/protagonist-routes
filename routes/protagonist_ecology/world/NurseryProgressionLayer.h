#pragma once

#include "core/interfaces/IWorldLayer.h"
#include "core/types/Aliases.h"
#include "core/types/Vec2.h"

#include <cstddef>
#include <cstdint>
#include <random>

namespace neuro {
class IWorld;
class ObjectLayer;
}

namespace neuro::routes::protagonist {

class BaseLayer;
class CampfireLayer;
class InterventionDirectorLayer;
class NurseryLogisticsLayer;
class WorksiteLayer;

// Stone Bootstrap commit 1 (STONE_BOOTSTRAP_PERSISTENCE_PROPOSAL.md
// option D): cross-episode unlock state for the protagonist nursery
// curriculum. Each episode currently constructs a fresh
// NurseryProgressionLayer with all unlock flags = false, so the
// protagonist must re-earn the "deposit 6 sticks → stone unlock"
// chain every single episode. This struct lets the route-level
// runtime carry the unlock state across episode boundaries (and
// eventually across processes via MainWorldPersistence).
//
// All fields are monotonic: once true, the caller should OR-merge so
// the flag stays true. The runtime owns one of these per archetype
// tag (only protagonist (tag=0) really uses it in practice).
struct ArchetypeProgressionState {
    bool first_fire_ignited{false};
    bool worksite_unlocked{false};
    bool stone_unlocked{false};
    bool heavy_unlocked{false};
    bool second_fire_ignited{false};
};

class NurseryProgressionLayer final : public IWorldLayer {
public:
    static constexpr const char* kName = "protagonist_nursery_progression";

    NurseryProgressionLayer(std::size_t first_fire_stick_threshold,
                            std::size_t worksite_unlock_deposit_threshold,
                            std::size_t stone_unlock_deposit_threshold,
                            std::size_t heavy_unlock_deposit_threshold,
                            std::size_t second_fire_deposit_threshold,
                            std::size_t worksite_unlock_stone_threshold,
                            std::size_t heavy_unlock_stone_threshold,
                            std::size_t second_fire_stone_threshold,
                            std::size_t stone_unlock_count,
                            std::size_t heavy_unlock_count,
                            float unlock_radius_min,
                            float unlock_radius_max,
                            SimTimeSeconds event_min_interval_seconds,
                            SimTimeSeconds event_max_interval_seconds,
                            std::uint32_t seed);

    const char* layerName() const override { return kName; }
    void onAttach(IWorld& world) override;
    void tick(IWorld& world, float dt) override;
    int tickOrder() const override { return 145; }

    // Stone Bootstrap commit 1: pre-seed the unlock flags from a
    // persisted archetype-level snapshot (e.g. kept by
    // ProtagonistEcologyRuntime across episode resets). Already-
    // unlocked flags skip their progression event; flags this layer
    // would otherwise have to re-earn from deposits stay false and
    // re-earn normally. Call BEFORE the first tick (typically right
    // after the layer is attached) for deterministic behavior.
    void seedFromProgressionState(const ArchetypeProgressionState& state);

    // Snapshot the current unlock state. The runtime calls this at
    // episode end and OR-merges into its persistent archetype state.
    ArchetypeProgressionState progressionState() const;

private:
    std::size_t promoteObjects(std::size_t count, bool heavy);

    enum class PendingEvent : std::uint8_t {
        FirstFire,
        WorksiteUnlock,
        StoneDrop,
        HeavyDrop,
        SecondFire,
    };

    bool hasPendingEvents() const;
    bool dispatchRandomPendingEvent();
    SimTimeSeconds drawEventDelaySeconds();

    BaseLayer* base_ = nullptr;
    ObjectLayer* objects_ = nullptr;
    CampfireLayer* campfire_ = nullptr;
    InterventionDirectorLayer* director_ = nullptr;
    NurseryLogisticsLayer* logistics_ = nullptr;
    WorksiteLayer* worksite_ = nullptr;
    std::size_t first_fire_stick_threshold_;
    std::size_t worksite_unlock_deposit_threshold_;
    std::size_t stone_unlock_deposit_threshold_;
    std::size_t heavy_unlock_deposit_threshold_;
    std::size_t second_fire_deposit_threshold_;
    std::size_t worksite_unlock_stone_threshold_;
    std::size_t heavy_unlock_stone_threshold_;
    std::size_t second_fire_stone_threshold_;
    std::size_t stone_unlock_count_;
    std::size_t heavy_unlock_count_;
    float unlock_radius_min_;
    float unlock_radius_max_;
    SimTimeSeconds event_min_interval_seconds_;
    SimTimeSeconds event_max_interval_seconds_;
    bool first_fire_ignited_ = false;
    bool worksite_unlocked_ = false;
    bool stone_unlocked_ = false;
    bool heavy_unlocked_ = false;
    bool second_fire_ignited_ = false;
    bool event_scheduled_ = false;
    Vec2 world_size_{};
    SimTimeSeconds next_event_time_seconds_ = 0.0;
    std::mt19937 rng_;
};

}  // namespace neuro::routes::protagonist
