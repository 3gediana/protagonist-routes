#include "capability/GoalActionAssistCapability.h"

#include "brain/GoalMemory.h"
#include "brain/ProtagonistBrain.h"
#include "core/agent/Agent.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/object/ObjectLayer.h"
#include "world/SurvivalStatusLayer.h"
#include "world/WaterLayer.h"
#include "world/WorksiteLayer.h"

#include <algorithm>

namespace neuro::routes::protagonist {

namespace {

constexpr double kMaxRecentSignalCueAgeSeconds = 30.0;

bool hasTarget(Vec2 target) {
    return target.x != 0.0f || target.y != 0.0f;
}

Vec2 goalTarget(const ProtagonistBrain& brain, GoalType goal, Vec2 position) {
    const auto& spatial = brain.spatialMemory();
    switch (goal) {
        case GoalType::FindWater:
            return spatial.findNearestWater(position, 0.15f);
        case GoalType::FindFood:
            return spatial.findNearestFood(position, 0.15f);
        case GoalType::GatherStick:
            return spatial.findNearestStick(position, 0.15f);
        case GoalType::GatherStone:
            return spatial.findNearestStone(position, 0.15f);
        case GoalType::ChopTree:
            return spatial.findNearestTree(position, 0.15f);
        case GoalType::BuildWorksite:
            return spatial.findNearestWorksite(position, 0.15f);
        case GoalType::RestNearFire:
            return spatial.findNearestFire(position, 0.15f);
        case GoalType::CraftWeapon:
            return spatial.findNearestBase(position, 0.15f);
        default:
            return {};
    }
}

bool hasIncompleteActiveWorksite(const WorksiteLayer* worksites) {
    if (worksites == nullptr) {
        return false;
    }
    for (const auto& site : worksites->sites()) {
        if (site.active && !site.completed) {
            return true;
        }
    }
    return false;
}

Vec2 recentSignalTarget(const ProtagonistBrain& brain, const IWorld& world) {
    const auto recent_signal = brain.episodicMemory().recentOfType("peer_signal_heard", 1);
    if (recent_signal.empty()) {
        return {};
    }
    const auto& signal = recent_signal.front();
    const double now = world.currentTimeSeconds();
    if (now <= signal.time_seconds || now - signal.time_seconds > kMaxRecentSignalCueAgeSeconds) {
        return {};
    }
    return signal.pos;
}

Vec2 carryReturnTarget(const ProtagonistBrain& brain, const IWorld& world, Vec2 position) {
    const auto& spatial = brain.spatialMemory();
    Vec2 target = spatial.findNearestWorksite(position, 0.15f);
    if (hasTarget(target)) {
        return target;
    }
    if (hasIncompleteActiveWorksite(world.getLayer<WorksiteLayer>())) {
        target = recentSignalTarget(brain, world);
        if (hasTarget(target)) {
            return target;
        }
    }
    return spatial.findNearestBase(position, 0.15f);
}

void steerToward(IAgent& agent, IWorld& world, Vec2 target, float assist_speed, float dt) {
    const Vec2 delta = target - agent.position();
    const float distance = delta.length();
    if (distance <= 0.0001f) {
        agent.setVelocity(kZeroVec2);
        return;
    }

    Vec2 direction = delta / distance;
    const float step = std::min(distance, std::max(0.0f, assist_speed) * std::max(0.0f, dt));
    Vec2 next = agent.position() + direction * step;
    const Vec2 world_size = world.size();
    next.x = std::clamp(next.x, 0.0f, world_size.x);
    next.y = std::clamp(next.y, 0.0f, world_size.y);
    agent.setPosition(next);
    agent.setVelocity(direction * (dt > 0.0f ? step / dt : 0.0f));
}

ObjectType desiredMaterialForGoal(GoalType goal) {
    return goal == GoalType::GatherStone ? ObjectType::Stone : ObjectType::Stick;
}

}  // namespace

GoalActionAssistCapability::GoalActionAssistCapability(float assist_speed, float pick_radius)
    : assist_speed_(assist_speed), pick_radius_(pick_radius) {}

void GoalActionAssistCapability::apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) {
    (void)outputs;
    if (dt <= 0.0f) {
        return;
    }

    auto* concrete = dynamic_cast<Agent*>(&agent);
    auto* brain = concrete != nullptr ? dynamic_cast<ProtagonistBrain*>(&concrete->brain()) : nullptr;
    if (concrete == nullptr || brain == nullptr) {
        return;
    }

    const auto goal = brain->goalMemory().currentGoal();
    if (goal == GoalType::Explore || goal == GoalType::Count) {
        return;
    }

    Vec2 target = goalTarget(*brain, goal, concrete->position());
    if (!concrete->isCarrying() && goal == GoalType::BuildWorksite) {
        target = {};
    } else if (concrete->isCarrying()
               && (goal == GoalType::GatherStick
                   || goal == GoalType::GatherStone
                   || goal == GoalType::BuildWorksite)) {
        target = carryReturnTarget(*brain, world, concrete->position());
    }
    if (hasTarget(target)) {
        steerToward(*concrete, world, target, assist_speed_, dt);
    }

    if (goal == GoalType::FindWater) {
        auto* water = world.getLayer<WaterLayer>();
        auto* survival = world.getLayer<SurvivalStatusLayer>();
        if (water != nullptr && survival != nullptr && water->inDrinkZone(concrete->position())) {
            survival->drink(concrete->id(), water->drinkRate() * dt);
        }
        return;
    }

    if (concrete->isCarrying() || (goal != GoalType::GatherStick && goal != GoalType::GatherStone)) {
        return;
    }

    auto* objects = world.getLayer<ObjectLayer>();
    if (objects == nullptr) {
        return;
    }

    const auto desired_type = desiredMaterialForGoal(goal);
    const auto nearby = objects->nearestObjects(concrete->position(), pick_radius_);
    for (const auto* candidate : nearby) {
        if (candidate == nullptr
            || candidate->type != desired_type
            || candidate->heavy
            || candidate->carriers_required > 1) {
            continue;
        }

        auto* object = objects->findById(candidate->id);
        if (object == nullptr) {
            continue;
        }

        object->carried = true;
        object->pos = concrete->position();
        object->velocity = kZeroVec2;
        concrete->setCarriedObject(object->id);
        world.eventBus().publish(events::Event{events::EventType::ObjectPickedUp,
                                               events::ObjectPickedUp{concrete->id(), object->id},
                                               world.currentTick(),
                                               world.currentTimeSeconds()});
        return;
    }
}

}  // namespace neuro::routes::protagonist
