#include "world/NurserySupplyLayer.h"

#include "core/interfaces/IWorld.h"
#include "core/world/layers/object/ObjectLayer.h"
#include "world/BaseLayer.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace neuro::routes::protagonist {

namespace {

Vec2 clampToWorld(Vec2 p, Vec2 world_size) {
    p.x = std::clamp(p.x, 0.0f, world_size.x);
    p.y = std::clamp(p.y, 0.0f, world_size.y);
    return p;
}

Vec2 keepOutsideStockpile(const BaseLayer& base, Vec2 candidate, Vec2 world_size) {
    candidate = clampToWorld(candidate, world_size);
    if (!base.inStockpile(candidate)) {
        return candidate;
    }

    Vec2 away = candidate - base.stockpileCenter();
    float length = away.length();
    if (length <= 0.0001f) {
        away = base.nestCenter() - base.stockpileCenter();
        length = away.length();
    }
    if (length <= 0.0001f) {
        away = Vec2{-1.0f, 0.0f};
        length = 1.0f;
    }
    away /= length;
    return clampToWorld(base.stockpileCenter() + away * (base.stockpileRadius() + 1.5f), world_size);
}

}  // namespace

NurserySupplyLayer::NurserySupplyLayer(std::size_t min_nearby_sticks,
                                       std::size_t replenish_batch,
                                       float near_radius,
                                       float spawn_radius,
                                       SimTimeSeconds cooldown_seconds,
                                       std::uint32_t seed)
    : min_nearby_sticks_(min_nearby_sticks),
      replenish_batch_(replenish_batch),
      near_radius_(near_radius),
      spawn_radius_(spawn_radius),
      cooldown_seconds_(std::max<SimTimeSeconds>(0.0, cooldown_seconds)),
      rng_(seed == 0 ? std::random_device{}() : seed) {}

void NurserySupplyLayer::onAttach(IWorld& world) {
    base_ = world.getLayer<BaseLayer>();
    objects_ = world.getLayer<ObjectLayer>();
}

void NurserySupplyLayer::tick(IWorld& world, float dt) {
    (void)dt;
    if (base_ == nullptr || objects_ == nullptr || replenish_batch_ == 0) {
        return;
    }
    if (hasLooseStone()) {
        return;
    }
    if (last_replenish_time_seconds_ >= 0.0
        && world.currentTimeSeconds() < last_replenish_time_seconds_ + cooldown_seconds_) {
        return;
    }
    if (nearbyLooseMaterials() >= min_nearby_sticks_) {
        return;
    }
    replenish(world);
    last_replenish_time_seconds_ = world.currentTimeSeconds();
}

bool NurserySupplyLayer::hasLooseStone() const {
    if (base_ == nullptr || objects_ == nullptr) {
        return false;
    }
    for (const auto& object : objects_->objects()) {
        if (object.carried || base_->inStockpile(object.pos)) {
            continue;
        }
        if (object.type == ObjectType::Stone) {
            return true;
        }
    }
    return false;
}

std::size_t NurserySupplyLayer::nearbyLooseMaterials() const {
    if (base_ == nullptr || objects_ == nullptr) {
        return 0;
    }
    const float near_radius_sq = near_radius_ * near_radius_;
    std::size_t count = 0;
    for (const auto& object : objects_->objects()) {
        if (object.carried) {
            continue;
        }
        if (base_->inStockpile(object.pos)) {
            continue;
        }
        if (Vec2::distanceSquared(object.pos, base_->nestCenter()) > near_radius_sq) {
            continue;
        }
        ++count;
    }
    return count;
}

void NurserySupplyLayer::replenish(IWorld& world) {
    std::vector<MovableObject*> candidates;
    candidates.reserve(objects_->objects().size());
    for (auto& object : objects_->objects()) {
        if (object.carried || object.type != ObjectType::Stick) {
            continue;
        }
        if (base_->inStockpile(object.pos)) {
            continue;
        }
        if (Vec2::distanceSquared(object.pos, base_->nestCenter()) <= near_radius_ * near_radius_) {
            continue;
        }
        candidates.push_back(&object);
    }
    if (candidates.empty()) {
        return;
    }

    std::shuffle(candidates.begin(), candidates.end(), rng_);
    std::uniform_real_distribution<float> angle_dist(0.0f, 6.28318530718f);
    std::uniform_real_distribution<float> radius_dist(1.5f, std::max(2.0f, spawn_radius_));
    const auto count = std::min<std::size_t>(replenish_batch_, candidates.size());
    for (std::size_t i = 0; i < count; ++i) {
        auto* object = candidates[i];
        const float angle = angle_dist(rng_);
        const float radius = radius_dist(rng_);
        object->type = ObjectType::Stick;
        object->heavy = false;
        object->mass = 0.9f;
        object->carriers_required = std::size_t(1);
        object->carried = false;
        object->velocity = kZeroVec2;
        object->pos = keepOutsideStockpile(*base_, base_->nestCenter() + Vec2{std::cos(angle) * radius, std::sin(angle) * radius}, world.size());
    }
}

}  // namespace neuro::routes::protagonist
