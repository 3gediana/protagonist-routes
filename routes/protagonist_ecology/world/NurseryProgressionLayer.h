#pragma once

#include "core/interfaces/IWorldLayer.h"
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
                            std::size_t event_min_interval_ticks,
                            std::size_t event_max_interval_ticks,
                            std::uint32_t seed);

    const char* layerName() const override { return kName; }
    void onAttach(IWorld& world) override;
    void tick(IWorld& world, float dt) override;
    int tickOrder() const override { return 145; }

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
    std::uint64_t drawEventDelayTicks();

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
    std::size_t event_min_interval_ticks_;
    std::size_t event_max_interval_ticks_;
    bool first_fire_ignited_ = false;
    bool worksite_unlocked_ = false;
    bool stone_unlocked_ = false;
    bool heavy_unlocked_ = false;
    bool second_fire_ignited_ = false;
    bool event_scheduled_ = false;
    Vec2 world_size_{};
    std::uint64_t next_event_tick_ = 0;
    std::mt19937 rng_;
};

}  // namespace neuro::routes::protagonist
