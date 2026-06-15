#pragma once

#include "core/interfaces/IWorldLayer.h"

#include <cstddef>

namespace neuro {
class AgentLayer;
class IWorld;
class ObjectLayer;
}

namespace neuro::routes::protagonist {

class BaseLayer;
class CampfireLayer;
class InterventionDirectorLayer;
class NurseryLogisticsLayer;
class SurvivalStatusLayer;
class TreeLayer;
class WorksiteLayer;

class SandboxStatusLayer final : public IWorldLayer {
public:
    static constexpr const char* kName = "protagonist_sandbox_status";

    explicit SandboxStatusLayer(std::size_t log_interval_ticks);

    const char* layerName() const override { return kName; }
    void onAttach(IWorld& world) override;
    void tick(IWorld& world, float dt) override;
    int tickOrder() const override { return 1000; }

private:
    void logStatus(const IWorld& world) const;

    AgentLayer* agents_ = nullptr;
    BaseLayer* base_ = nullptr;
    CampfireLayer* campfire_ = nullptr;
    InterventionDirectorLayer* director_ = nullptr;
    NurseryLogisticsLayer* logistics_ = nullptr;
    ObjectLayer* objects_ = nullptr;
    SurvivalStatusLayer* survival_ = nullptr;
    TreeLayer* tree_ = nullptr;
    WorksiteLayer* worksite_ = nullptr;
    std::size_t log_interval_ticks_;
};

}  // namespace neuro::routes::protagonist
