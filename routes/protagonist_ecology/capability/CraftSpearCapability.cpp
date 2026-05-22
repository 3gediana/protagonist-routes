#include "capability/CraftSpearCapability.h"

#include "core/agent/Agent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/object/ObjectLayer.h"
#include "world/BaseLayer.h"
#include "world/NurseryLogisticsLayer.h"
#include "world/WorksiteLayer.h"

#include <algorithm>

namespace neuro::routes::protagonist {

CraftSpearCapability::CraftSpearCapability(float energy_cost, float activation_threshold)
    : energy_cost_(energy_cost), activation_threshold_(activation_threshold) {}

void CraftSpearCapability::apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) {
    if (outputs.empty() || outputs[0] < activation_threshold_) {
        return;
    }
    craft_attempts.fetch_add(1, std::memory_order_relaxed);

    auto* concrete = dynamic_cast<Agent*>(&agent);
    auto* base = world.getLayer<BaseLayer>();
    auto* objects = world.getLayer<ObjectLayer>();
    auto* logistics = world.getLayer<NurseryLogisticsLayer>();
    auto* worksites = world.getLayer<WorksiteLayer>();
    if (concrete == nullptr || base == nullptr || objects == nullptr || logistics == nullptr) {
        return;
    }
    if (!base->inBase(concrete->position())) {
        return;
    }

    auto find_loose_base_material = [&](ObjectType type) -> MovableObject* {
        for (auto& object : objects->objects()) {
            if (object.carried || object.type != type) {
                continue;
            }
            if (!base->inBase(object.pos) || Vec2::distanceSquared(object.pos, concrete->position()) > 8.0f * 8.0f) {
                continue;
            }
            return &object;
        }
        return nullptr;
    };

    auto* carried = concrete->isCarrying() ? objects->findById(concrete->carriedObject()) : nullptr;
    if (carried != nullptr && carried->type != ObjectType::Stick) {
        return;
    }

    std::size_t loose_stone_count = 0;
    for (const auto& object : objects->objects()) {
        if (!object.carried && object.type == ObjectType::Stone) {
            ++loose_stone_count;
        }
    }
    std::size_t reserved_worksite_stones = 0;
    if (worksites != nullptr) {
        for (const auto& site : worksites->sites()) {
            if (!site.active || site.completed || site.stored_stones >= site.required_stones) {
                continue;
            }
            reserved_worksite_stones += site.required_stones - site.stored_stones;
        }
    }
    const std::size_t total_available_stones = logistics->stockpiledCount(ObjectType::Stone) + loose_stone_count;
    if (reserved_worksite_stones > 0 && total_available_stones <= reserved_worksite_stones) {
        return;
    }

    const bool have_stick_source = (carried != nullptr && carried->type == ObjectType::Stick)
                                || (find_loose_base_material(ObjectType::Stick) != nullptr)
                                || (logistics->stockpiledCount(ObjectType::Stick) > 0);
    const bool have_stone_source = (find_loose_base_material(ObjectType::Stone) != nullptr)
                                || (logistics->stockpiledCount(ObjectType::Stone) > 0);
    if (!have_stick_source || !have_stone_source) {
        return;
    }

    bool consumed_stone = logistics->consumeStockpiledObject(ObjectType::Stone);
    if (!consumed_stone) {
        if (auto* object = find_loose_base_material(ObjectType::Stone); object != nullptr) {
            object->carried = true;
            object->velocity = kZeroVec2;
            object->pos = Vec2{-1024.0f, -1024.0f};
            consumed_stone = true;
        }
    }
    if (!consumed_stone) {
        return;
    }

    if (carried != nullptr && carried->type == ObjectType::Stick) {
        carried->type = ObjectType::Spear;
        carried->mass = 1.3f;
        carried->heavy = false;
        carried->carriers_required = std::size_t(1);
    } else if (auto* loose_stick = find_loose_base_material(ObjectType::Stick); loose_stick != nullptr) {
        loose_stick->type = ObjectType::Spear;
        loose_stick->mass = 1.3f;
        loose_stick->heavy = false;
        loose_stick->carriers_required = std::size_t(1);
        loose_stick->carried = true;
        loose_stick->velocity = kZeroVec2;
        loose_stick->pos = concrete->position();
        concrete->setCarriedObject(loose_stick->id);
    } else if (logistics->consumeStockpiledObject(ObjectType::Stick)) {
        const auto spear_id = objects->nextId();
        objects->objects().push_back(MovableObject{
            spear_id,
            concrete->position(),
            kZeroVec2,
            1.3f,
            ObjectType::Spear,
            true,
            false,
            std::size_t(1)
        });
        concrete->setCarriedObject(spear_id);
    } else {
        return;
    }

    concrete->setEnergy(concrete->energy() - energy_cost_ * dt);
    craft_successes.fetch_add(1, std::memory_order_relaxed);
    // L2.7 v13 HRL sub-reward: CraftWeapon (GoalType=7) completed.
    concrete->noteGoalCompletion(7);
}

}  // namespace neuro::routes::protagonist
