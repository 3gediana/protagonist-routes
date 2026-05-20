#include "core/world/layers/ResourceLayer.h"

#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/terrain/TerrainLayer.h"
#include "core/world/layers/SeasonLayer.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace neuro {

ResourceLayer::ResourceLayer(std::size_t resource_count,
                             float resource_amount,
                             SimTimeSeconds respawn_seconds,
                             std::uint32_t seed,
                             bool dynamic,
                             float regen_rate,
                             bool terrain_adjustment_enabled)
    : resource_count_(resource_count),
      resource_amount_(resource_amount),
      respawn_seconds_(std::max<SimTimeSeconds>(0.0, respawn_seconds)),
      dynamic_(dynamic),
      regen_rate_(regen_rate),
      terrain_adjustment_enabled_(terrain_adjustment_enabled),
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
                                               world.currentTick(),
                                               world.currentTimeSeconds()});
    }
}

void ResourceLayer::tick(IWorld& world, float dt) {
    // Season affects resource regeneration rate
    float season_regen = 1.0f;
    const auto* season = world.getLayer<SeasonLayer>();
    if (season) {
        season_regen = season->resourceFactor();
    }

    for (auto& resource : resources_) {
        if (dynamic_ && resource.available && resource.amount < resource.max_amount) {
            resource.amount = std::min(resource.amount + resource.regen_rate * season_regen * dt, resource.max_amount);
        }
        if (resource.available || resource.respawn_seconds_remaining <= 0.0) {
            continue;
        }
        resource.respawn_seconds_remaining = std::max<SimTimeSeconds>(0.0, resource.respawn_seconds_remaining - static_cast<SimTimeSeconds>(dt));
        if (resource.respawn_seconds_remaining <= 0.0) {
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

float ResourceLayer::tryConsume(Vec2 position, float radius, IWorld& world, float amount_limit) {
    return tryConsumeTyped(position, radius, world, amount_limit).amount;
}

ResourceLayer::ConsumeResult ResourceLayer::tryConsumeTyped(Vec2 position, float radius, IWorld& world, float amount_limit) {
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
        return ConsumeResult{};
    }

    auto& resource = resources_[best_index];
    const bool was_meat = resource.is_meat;
    const float consume_limit = amount_limit > 0.0f ? amount_limit : resource.amount;
    const float consumed = std::min(resource.amount, consume_limit);
    if (consumed <= 0.0f) {
        return ConsumeResult{};
    }

    // Transient corpses are not regenerated after consumption.
    const SimTimeSeconds respawn_after = resource.is_transient ? 0.0 : respawn_seconds_;

    resource.amount = std::max(0.0f, resource.amount - consumed);
    if (resource.amount <= 1.0e-6f) {
        resource.amount = 0.0f;
        resource.available = false;
        resource.respawn_seconds_remaining = respawn_after;
        world.eventBus().publish(events::Event{events::EventType::ResourceConsumed,
                                               events::ResourceConsumed{resource.position},
                                               world.currentTick(),
                                               world.currentTimeSeconds()});
        return ConsumeResult{consumed, was_meat};
    }

    if (dynamic_) {
        resource.available = true;
        return ConsumeResult{consumed, was_meat};
    }

    resource.available = true;
    return ConsumeResult{consumed, was_meat};
}

void ResourceLayer::addCorpse(Vec2 position, float amount, IWorld& world) {
    if (amount <= 0.0f) return;
    Resource corpse{};
    corpse.position = position;
    corpse.amount = amount;
    corpse.max_amount = amount;
    corpse.regen_rate = 0.0f;
    corpse.available = true;
    corpse.respawn_seconds_remaining = 0.0;
    corpse.is_meat = true;
    corpse.is_transient = true;
    resources_.push_back(corpse);
    world.eventBus().publish(events::Event{events::EventType::ResourceSpawned,
                                           events::ResourceSpawned{position, amount},
                                           world.currentTick(),
                                           world.currentTimeSeconds()});
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
                                           world.currentTick(),
                                           world.currentTimeSeconds()});
}

float ResourceLayer::terrainAdjustedAmount(Vec2 position, IWorld& world) const {
    float amount = resource_amount_;
    if (!terrain_adjustment_enabled_) {
        return amount;
    }
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
