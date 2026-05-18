#include "core/world/layers/ResourceLayer.h"

#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/terrain/TerrainLayer.h"

#include <cmath>
#include <limits>

namespace neuro {

ResourceLayer::ResourceLayer(std::size_t resource_count,
                             float resource_amount,
                             std::size_t respawn_ticks,
                             std::uint32_t seed,
                             bool dynamic,
                             float regen_rate)
    : resource_count_(resource_count),
      resource_amount_(resource_amount),
      respawn_ticks_(respawn_ticks),
      dynamic_(dynamic),
      regen_rate_(regen_rate),
      rng_(seed == 0 ? std::random_device{}() : seed),
      world_size_(0.0f, 0.0f) {}

const char* ResourceLayer::layerName() const {
    return kName;
}

void ResourceLayer::onAttach(IWorld& world) {
    world_size_ = world.size();
    resources_.clear();
    resources_.reserve(resource_count_);
    for (std::size_t i = 0; i < resource_count_; ++i) {
        const float adj = terrainAdjustedAmount(randomPosition(world_size_), world);
        Resource resource{randomPosition(world_size_), adj, adj, regen_rate_, true, 0};
        resources_.push_back(resource);
        world.eventBus().publish(events::Event{events::EventType::ResourceSpawned,
                                               events::ResourceSpawned{resource.position, resource.amount},
                                               world.currentTick()});
    }
}

void ResourceLayer::tick(IWorld& world, float dt) {
    for (auto& resource : resources_) {
        if (dynamic_ && resource.available && resource.amount < resource.max_amount) {
            resource.amount = std::min(resource.amount + resource.regen_rate * dt, resource.max_amount);
        }
        if (resource.available || resource.respawn_ticks_remaining == 0) {
            continue;
        }
        --resource.respawn_ticks_remaining;
        if (resource.respawn_ticks_remaining == 0) {
            respawnResource(resource, world);
        }
    }
}

int ResourceLayer::tickOrder() const {
    return 0;
}

std::optional<ResourceLayer::NearestResource> ResourceLayer::nearestAvailable(Vec2 position) const {
    std::optional<NearestResource> best;
    float best_distance_sq = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < resources_.size(); ++i) {
        const auto& resource = resources_[i];
        if (!resource.available) {
            continue;
        }
        const float distance_sq = Vec2::distanceSquared(position, resource.position);
        if (distance_sq < best_distance_sq) {
            best_distance_sq = distance_sq;
            best = NearestResource{i, resource.position, resource.amount, std::sqrt(distance_sq)};
        }
    }
    return best;
}

float ResourceLayer::tryConsume(Vec2 position, float radius, IWorld& world) {
    const float radius_sq = radius * radius;
    std::size_t best_index = resources_.size();
    float best_distance_sq = radius_sq;

    for (std::size_t i = 0; i < resources_.size(); ++i) {
        auto& resource = resources_[i];
        if (!resource.available) {
            continue;
        }
        const float distance_sq = Vec2::distanceSquared(position, resource.position);
        if (distance_sq <= best_distance_sq) {
            best_distance_sq = distance_sq;
            best_index = i;
        }
    }

    if (best_index == resources_.size()) {
        return 0.0f;
    }

    auto& resource = resources_[best_index];
    if (dynamic_) {
        const float consumed = resource.amount;
        resource.amount = 0.0f;
        resource.available = false;
        resource.respawn_ticks_remaining = respawn_ticks_;
        world.eventBus().publish(events::Event{events::EventType::ResourceConsumed,
                                               events::ResourceConsumed{resource.position},
                                               world.currentTick()});
        return consumed;
    }
    resource.available = false;
    resource.respawn_ticks_remaining = respawn_ticks_;
    world.eventBus().publish(events::Event{events::EventType::ResourceConsumed,
                                           events::ResourceConsumed{resource.position},
                                           world.currentTick()});
    return resource.amount;
}

const std::vector<ResourceLayer::Resource>& ResourceLayer::resources() const {
    return resources_;
}

Vec2 ResourceLayer::randomPosition(Vec2 world_size) {
    std::uniform_real_distribution<float> x_dist(0.0f, world_size.x);
    std::uniform_real_distribution<float> y_dist(0.0f, world_size.y);
    return {x_dist(rng_), y_dist(rng_)};
}

void ResourceLayer::respawnResource(Resource& resource, IWorld& world) {
    resource.position = randomPosition(world_size_);
    const float adj = terrainAdjustedAmount(resource.position, world);
    resource.amount = adj;
    resource.max_amount = adj;
    resource.regen_rate = regen_rate_;
    resource.available = true;
    world.eventBus().publish(events::Event{events::EventType::ResourceSpawned,
                                           events::ResourceSpawned{resource.position, resource.amount},
                                           world.currentTick()});
}

float ResourceLayer::terrainAdjustedAmount(Vec2 position, IWorld& world) const {
    float amount = resource_amount_;
    const auto* terrain = world.findLayer(TerrainLayer::kName);
    if (terrain != nullptr) {
        const auto* tl = dynamic_cast<const TerrainLayer*>(terrain);
        if (tl != nullptr) {
            const float h = tl->heightAt(position.x, position.y);
            const float h_ratio = h / tl->heightScale();
            if (h_ratio > 0.7f) {
                amount *= 2.0f;
            }
            if (tl->inCave(position.x, position.y)) {
                amount *= 3.0f;
            }
        }
    }
    return amount;
}

}  // namespace neuro
