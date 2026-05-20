#include "world/NurseryLogisticsLayer.h"

#include "core/agent/Agent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"
#include "world/BaseLayer.h"

#include <algorithm>
#include <cmath>

namespace neuro::routes::protagonist {

namespace {

std::size_t typeIndex(ObjectType type) {
    return static_cast<std::size_t>(type);
}

Vec2 radialOffset(std::mt19937& rng, float radius) {
    std::uniform_real_distribution<float> angle_dist(0.0f, 6.28318530718f);
    std::uniform_real_distribution<float> radius_dist(0.15f, 1.0f);
    const float angle = angle_dist(rng);
    const float scaled_radius = radius * radius_dist(rng);
    return Vec2{std::cos(angle) * scaled_radius, std::sin(angle) * scaled_radius};
}

}  // namespace

NurseryLogisticsLayer::NurseryLogisticsLayer(float deposit_energy_bonus,
                                             float deposit_fitness_bonus,
                                             float stockpile_snap_radius,
                                             std::uint32_t seed)
    : deposit_energy_bonus_(deposit_energy_bonus),
      deposit_fitness_bonus_(deposit_fitness_bonus),
      stockpile_snap_radius_(stockpile_snap_radius),
      rng_(seed == 0 ? std::random_device{}() : seed) {}

void NurseryLogisticsLayer::onAttach(IWorld& world) {
    base_ = world.getLayer<BaseLayer>();
    objects_ = world.getLayer<ObjectLayer>();
    agents_ = world.getLayer<AgentLayer>();
    refreshEligibility();
}

void NurseryLogisticsLayer::tick(IWorld& world, float dt) {
    (void)world;
    (void)dt;
    if (base_ == nullptr || objects_ == nullptr || agents_ == nullptr) {
        return;
    }

    refreshEligibility();
    processCarriedDeposits();
}

std::size_t NurseryLogisticsLayer::stockpiledCount(ObjectType type) const {
    const auto index = typeIndex(type);
    return index < stockpiled_by_type_.size() ? stockpiled_by_type_[index] : 0u;
}

std::size_t NurseryLogisticsLayer::totalDeposits(ObjectType type) const {
    const auto index = typeIndex(type);
    return index < total_deposited_by_type_.size() ? total_deposited_by_type_[index] : 0u;
}

bool NurseryLogisticsLayer::consumeStockpiledObject(ObjectType type) {
    if (base_ == nullptr || objects_ == nullptr) {
        return false;
    }

    for (auto& object : objects_->objects()) {
        if (object.carried || object.type != type || !base_->inStockpile(object.pos)) {
            continue;
        }

        stockpiled_object_ids_.erase(object.id);
        const auto index = typeIndex(type);
        if (index < stockpiled_by_type_.size() && stockpiled_by_type_[index] > 0) {
            --stockpiled_by_type_[index];
        }
        object.carried = true;
        object.velocity = kZeroVec2;
        object.pos = Vec2{-1024.0f, -1024.0f};
        deposit_eligible_[object.id] = false;
        return true;
    }

    return false;
}

void NurseryLogisticsLayer::refreshEligibility() {
    if (base_ == nullptr || objects_ == nullptr) {
        return;
    }

    for (const auto& object : objects_->objects()) {
        if (!deposit_eligible_.contains(object.id)) {
            deposit_eligible_.emplace(object.id, true);
        }

        const bool currently_stockpiled = !object.carried && base_->inStockpile(object.pos);
        const bool tracked_stockpiled = stockpiled_object_ids_.contains(object.id);
        if (currently_stockpiled && !tracked_stockpiled) {
            stockpiled_object_ids_.insert(object.id);
            const auto index = typeIndex(object.type);
            if (index < stockpiled_by_type_.size()) {
                ++stockpiled_by_type_[index];
            }
        } else if (!currently_stockpiled && tracked_stockpiled) {
            stockpiled_object_ids_.erase(object.id);
            const auto index = typeIndex(object.type);
            if (index < stockpiled_by_type_.size() && stockpiled_by_type_[index] > 0) {
                --stockpiled_by_type_[index];
            }
        }

        if (!object.carried && !base_->inStockpile(object.pos)) {
            deposit_eligible_[object.id] = true;
        }
    }
}

void NurseryLogisticsLayer::processCarriedDeposits() {
    for (Agent* agent : agents_->agents()) {
        if (agent == nullptr || !agent->alive() || !agent->isCarrying()) {
            continue;
        }
        if (!base_->inStockpile(agent->position())) {
            continue;
        }

        auto* object = objects_->findById(agent->carriedObject());
        if (object == nullptr) {
            agent->setCarriedObject(0);
            continue;
        }

        auto it = deposit_eligible_.find(object->id);
        if (it == deposit_eligible_.end() || !it->second) {
            continue;
        }

        depositObject(*agent, *object);
        it->second = false;
    }
}

void NurseryLogisticsLayer::depositObject(Agent& agent, MovableObject& object) {
    object.carried = false;
    object.velocity = kZeroVec2;
    object.pos = base_->stockpileCenter() + radialOffset(rng_, stockpile_snap_radius_);
    agent.setCarriedObject(0);
    agent.setEnergy(agent.energy() + deposit_energy_bonus_);
    agent.addFitness(deposit_fitness_bonus_);
    ++total_deposits_;
    const auto index = typeIndex(object.type);
    if (index < total_deposited_by_type_.size()) {
        ++total_deposited_by_type_[index];
    }

    refreshEligibility();
}

}  // namespace neuro::routes::protagonist
