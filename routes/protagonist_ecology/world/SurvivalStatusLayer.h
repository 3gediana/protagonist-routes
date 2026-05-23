#pragma once

#include "core/interfaces/IWorldLayer.h"
#include "core/types/Aliases.h"

#include <cstddef>
#include <unordered_map>

namespace neuro {
class AgentLayer;
class IWorld;
}

namespace neuro::routes::protagonist {

class BaseLayer;
class CampfireLayer;
class WaterLayer;

class SurvivalStatusLayer final : public IWorldLayer {
public:
    static constexpr const char* kName = "protagonist_survival_status";

    SurvivalStatusLayer(float max_health,
                        float max_hydration,
                        float initial_health_ratio,
                        float initial_hydration_ratio,
                        float hydration_decay_per_second,
                        float dehydration_threshold_ratio,
                        float dehydration_damage_per_second,
                        float fire_health_damage_per_second,
                        float warmth_recovery_per_second,
                        float nest_recovery_per_second,
                        float injured_threshold_ratio,
                        // === Layer 1 (Final Blueprint, 2026-05-23) ===
                        // R-001: extend physiology to all living entities
                        // (not protagonist-only). Background species
                        // can't actively drink (no DrinkWaterCapability),
                        // so they use a separate lower decay rate to
                        // avoid mass dehydration deaths.
                        bool track_all_living = true,
                        float background_hydration_decay_per_second = 0.0f,
                        // R-001: hunger-driven hp damage (high hunger
                        // starts attacking health). Reads Agent.drives_.hunger.
                        // Tuned 2026-05-24 from L1 smoke (alive=0): threshold
                        // 0.7 -> 0.9 (only severe hunger), damage 0.25 -> 0.08
                        // (3x slower) so it acts as a floor not a knife.
                        float hunger_damage_threshold = 0.9f,    // start at 90% hunger
                        float hunger_damage_per_second = 0.08f,  // hp/s at full hunger
                        // R-001: core_temperature-driven hp damage.
                        // Reads Agent.coreTemperature() (°C).
                        // Tuned 2026-05-24: widened band (was 32/42 -> 25/45)
                        // to give protagonist room to discover fire / nest
                        // before thermal damage kicks in; damage 0.4 -> 0.15.
                        float cold_temp_threshold = 25.0f,        // hypothermia onset
                        float hot_temp_threshold = 45.0f,         // hyperthermia onset
                        float thermal_damage_per_second = 0.15f);

    const char* layerName() const override { return kName; }
    void onAttach(IWorld& world) override;
    void tick(IWorld& world, float dt) override;
    int tickOrder() const override { return 95; }

    float hydrationRatio(AgentId id) const;
    float healthRatio(AgentId id) const;
    bool isDehydrated(AgentId id) const;
    bool isInjured(AgentId id) const;
    void drink(AgentId id, float amount);

    std::size_t trackedCount() const;
    std::size_t dehydratedCount() const;
    std::size_t injuredCount() const;
    float averageHydrationRatio() const;
    float averageHealthRatio() const;

private:
    struct SurvivalState {
        float health;
        float hydration;
    };

    void syncTrackedAgents();

    AgentLayer* agents_ = nullptr;
    BaseLayer* base_ = nullptr;
    CampfireLayer* campfire_ = nullptr;
    WaterLayer* water_ = nullptr;
    float max_health_;
    float max_hydration_;
    float initial_health_ratio_;
    float initial_hydration_ratio_;
    float hydration_decay_per_second_;
    float dehydration_threshold_ratio_;
    float dehydration_damage_per_second_;
    float fire_health_damage_per_second_;
    float warmth_recovery_per_second_;
    float nest_recovery_per_second_;
    float injured_threshold_ratio_;
    // Layer 1 (Final Blueprint) extensions.
    bool track_all_living_;
    float background_hydration_decay_per_second_;
    float hunger_damage_threshold_;
    float hunger_damage_per_second_;
    float cold_temp_threshold_;
    float hot_temp_threshold_;
    float thermal_damage_per_second_;
    std::unordered_map<AgentId, SurvivalState> states_;
};

}  // namespace neuro::routes::protagonist
