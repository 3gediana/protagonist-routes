#include "core/world/layers/object/ObjectLayer.h"

#include "core/interfaces/IWorld.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace neuro {

ObjectLayer::ObjectLayer(Vec2 world_size, std::size_t object_count, float damping)
    : world_size_(world_size), damping_(damping) {
    objects_.reserve(object_count);
    // Generate initial objects (stones)
    std::mt19937 rng(12345);
    std::uniform_real_distribution<float> x_dist(5.0f, world_size.x - 5.0f);
    std::uniform_real_distribution<float> y_dist(5.0f, world_size.y - 5.0f);
    for (std::size_t i = 0; i < object_count; ++i) {
        // First 30% of objects are heavy (require 2 carriers)
        const bool is_heavy = i < object_count / 3;
        objects_.push_back(MovableObject{
            next_id_++,
            Vec2{x_dist(rng), y_dist(rng)},
            Vec2{0.0f, 0.0f},
            is_heavy ? 5.0f : 1.0f,
            ObjectType::Stone,
            false,
            is_heavy,
            is_heavy ? std::size_t(2) : std::size_t(1)
        });
    }
}

void ObjectLayer::onAttach(IWorld& world) {
    (void)world;
}

void ObjectLayer::tick(IWorld& world, float dt) {
    (void)world;
    for (auto& obj : objects_) {
        if (obj.carried) continue;

        // Apply damping
        obj.velocity.x *= damping_;
        obj.velocity.y *= damping_;

        // Move
        obj.pos.x += obj.velocity.x * dt;
        obj.pos.y += obj.velocity.y * dt;

        // Boundary bounce
        if (obj.pos.x < 0.0f) { obj.pos.x = 0.0f; obj.velocity.x = 0.0f; }
        if (obj.pos.y < 0.0f) { obj.pos.y = 0.0f; obj.velocity.y = 0.0f; }
        if (obj.pos.x > world_size_.x) { obj.pos.x = world_size_.x; obj.velocity.x = 0.0f; }
        if (obj.pos.y > world_size_.y) { obj.pos.y = world_size_.y; obj.velocity.y = 0.0f; }

        // Stop if velocity very small
        if (std::abs(obj.velocity.x) < 0.01f) obj.velocity.x = 0.0f;
        if (std::abs(obj.velocity.y) < 0.01f) obj.velocity.y = 0.0f;
    }
}

MovableObject* ObjectLayer::findById(ObjectId id) {
    for (auto& obj : objects_) {
        if (obj.id == id) return &obj;
    }
    return nullptr;
}

const MovableObject* ObjectLayer::findById(ObjectId id) const {
    for (const auto& obj : objects_) {
        if (obj.id == id) return &obj;
    }
    return nullptr;
}

std::vector<const MovableObject*> ObjectLayer::nearestObjects(Vec2 pos, float radius) const {
    std::vector<const MovableObject*> result;
    const float r2 = radius * radius;
    for (const auto& obj : objects_) {
        if (obj.carried) continue;
        const float dx = obj.pos.x - pos.x;
        const float dy = obj.pos.y - pos.y;
        if (dx * dx + dy * dy <= r2) {
            result.push_back(&obj);
        }
    }
    // Sort by distance
    std::sort(result.begin(), result.end(), [&pos](const MovableObject* a, const MovableObject* b) {
        const float da = (a->pos.x - pos.x) * (a->pos.x - pos.x) + (a->pos.y - pos.y) * (a->pos.y - pos.y);
        const float db = (b->pos.x - pos.x) * (b->pos.x - pos.x) + (b->pos.y - pos.y) * (b->pos.y - pos.y);
        return da < db;
    });
    return result;
}

}  // namespace neuro
