#pragma once

#include "core/interfaces/IWorldLayer.h"
#include "core/world/layers/object/ObjectLayer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <unordered_map>
#include <unordered_set>

namespace neuro {
class Agent;
class AgentLayer;
}

namespace neuro::routes::protagonist {

class BaseLayer;

class NurseryLogisticsLayer final : public IWorldLayer {
public:
    static constexpr const char* kName = "protagonist_nursery_logistics";

    NurseryLogisticsLayer(float deposit_energy_bonus,
                          float deposit_fitness_bonus,
                          float stockpile_snap_radius,
                          std::uint32_t seed);

    const char* layerName() const override { return kName; }
    void onAttach(IWorld& world) override;
    void tick(IWorld& world, float dt) override;
    int tickOrder() const override { return 140; }

    std::size_t totalDeposits() const { return total_deposits_; }
    std::size_t totalDeposits(ObjectType type) const;
    std::size_t currentStockpiledCount() const { return stockpiled_object_ids_.size(); }
    std::size_t stockpiledCount(ObjectType type) const;
    bool consumeStockpiledObject(ObjectType type);

private:
    void refreshEligibility();
    void processCarriedDeposits();
    void depositObject(Agent& agent, MovableObject& object);

    BaseLayer* base_ = nullptr;
    ObjectLayer* objects_ = nullptr;
    AgentLayer* agents_ = nullptr;
    std::unordered_map<ObjectId, bool> deposit_eligible_;
    std::unordered_set<ObjectId> stockpiled_object_ids_;
    std::array<std::size_t, static_cast<std::size_t>(ObjectType::Count)> stockpiled_by_type_{};
    std::array<std::size_t, static_cast<std::size_t>(ObjectType::Count)> total_deposited_by_type_{};
    float deposit_energy_bonus_;
    float deposit_fitness_bonus_;
    float stockpile_snap_radius_;
    std::size_t total_deposits_ = 0;
    std::mt19937 rng_;
};

}  // namespace neuro::routes::protagonist
