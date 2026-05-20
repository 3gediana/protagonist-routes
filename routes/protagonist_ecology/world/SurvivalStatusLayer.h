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
                        float hydration_decay_per_tick,
                        float dehydration_threshold_ratio,
                        float dehydration_damage_per_tick,
                        float fire_health_damage_per_tick,
                        float warmth_recovery_per_tick,
                        float nest_recovery_per_tick,
                        float injured_threshold_ratio);

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
    float hydration_decay_per_tick_;
    float dehydration_threshold_ratio_;
    float dehydration_damage_per_tick_;
    float fire_health_damage_per_tick_;
    float warmth_recovery_per_tick_;
    float nest_recovery_per_tick_;
    float injured_threshold_ratio_;
    std::unordered_map<AgentId, SurvivalState> states_;
};

}  // namespace neuro::routes::protagonist
