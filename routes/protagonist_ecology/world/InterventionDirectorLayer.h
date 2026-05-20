#pragma once

#include "core/interfaces/IWorldLayer.h"
#include "core/types/Aliases.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <random>

namespace neuro {
class IWorld;
class AgentLayer;
class ObjectLayer;
}

namespace neuro::routes::protagonist {

class BaseLayer;
class CampfireLayer;
class WorksiteLayer;

class InterventionDirectorLayer final : public IWorldLayer {
public:
    static constexpr const char* kName = "protagonist_intervention_director";

    enum class RequestKind : std::uint8_t {
        IgniteCampfire,
        ActivateWorksite,
        ScatterStone,
        ScatterHeavyStone,
    };

    struct Request {
        RequestKind kind;
        std::string source;
        float cost;
        float stress;
    };

    InterventionDirectorLayer(float initial_budget,
                              float max_budget,
                              float recharge_per_second,
                              float stress_decay_per_second,
                              float max_stress,
                              SimTimeSeconds cooldown_seconds,
                              std::uint32_t seed);

    const char* layerName() const override { return kName; }
    void onAttach(IWorld& world) override;
    void tick(IWorld& world, float dt) override;
    int tickOrder() const override { return 146; }

    void submit(Request request);
    std::size_t pendingCount() const { return pending_.size(); }
    float budget() const { return budget_; }
    float stress() const { return stress_; }

private:
    bool canExecute(const Request& request, const IWorld& world) const;
    bool execute(const Request& request, IWorld& world);

    AgentLayer* agents_ = nullptr;
    ObjectLayer* objects_ = nullptr;
    BaseLayer* base_ = nullptr;
    CampfireLayer* campfire_ = nullptr;
    WorksiteLayer* worksite_ = nullptr;
    float budget_;
    float max_budget_;
    float recharge_per_second_;
    float stress_ = 0.0f;
    float stress_decay_per_second_;
    float max_stress_;
    SimTimeSeconds cooldown_seconds_;
    SimTimeSeconds last_execution_time_seconds_ = -1.0;
    std::deque<Request> pending_;
    std::mt19937 rng_;
};

}  // namespace neuro::routes::protagonist
