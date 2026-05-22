#include "world/BaseLayer.h"

#include "core/agent/Agent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"

#include <algorithm>

namespace neuro::routes::protagonist {

BaseLayer::BaseLayer(Vec2 center,
                     float base_radius,
                     float stockpile_radius,
                     float nest_radius,
                     float stockpile_offset,
                     float nest_offset,
                     float nest_predator_damage_per_second)
    : base_center_(center),
      stockpile_center_(center),
      nest_center_(center),
      base_radius_(base_radius),
      stockpile_radius_(stockpile_radius),
      nest_radius_(nest_radius),
      stockpile_offset_(stockpile_offset),
      nest_offset_(nest_offset),
      nest_predator_damage_per_second_(std::max(0.0f, nest_predator_damage_per_second)) {}

void BaseLayer::onAttach(IWorld& world) {
    base_center_ = clampToWorld(base_center_, world.size());
    stockpile_center_ = clampToWorld(Vec2{base_center_.x + stockpile_offset_, base_center_.y}, world.size());
    nest_center_ = clampToWorld(Vec2{base_center_.x + nest_offset_, base_center_.y}, world.size());
}

void BaseLayer::tick(IWorld& world, float dt) {
    if (nest_predator_damage_per_second_ <= 0.0f || dt <= 0.0f) {
        return;
    }
    auto* agents = world.getLayer<AgentLayer>();
    if (agents == nullptr) {
        return;
    }
    const float dmg = nest_predator_damage_per_second_ * dt;
    for (auto* agent : agents->agents()) {
        if (agent == nullptr || !agent->alive()) {
            continue;
        }
        if (!agent->isPredator()) {
            continue;
        }
        if (!inNest(agent->position())) {
            continue;
        }
        agent->setEnergy(agent->energy() - dmg);
    }
}

bool BaseLayer::inBase(Vec2 pos) const {
    return Vec2::distanceSquared(pos, base_center_) <= base_radius_ * base_radius_;
}

bool BaseLayer::inStockpile(Vec2 pos) const {
    return Vec2::distanceSquared(pos, stockpile_center_) <= stockpile_radius_ * stockpile_radius_;
}

bool BaseLayer::inNest(Vec2 pos) const {
    return Vec2::distanceSquared(pos, nest_center_) <= nest_radius_ * nest_radius_;
}

Vec2 BaseLayer::clampToWorld(Vec2 p, Vec2 world_size) const {
    p.x = std::clamp(p.x, 0.0f, world_size.x);
    p.y = std::clamp(p.y, 0.0f, world_size.y);
    return p;
}

}  // namespace neuro::routes::protagonist
