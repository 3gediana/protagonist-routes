#include "world/TreeLayer.h"

#include "core/interfaces/IWorld.h"
#include "core/world/layers/object/ObjectLayer.h"
#include "core/world/layers/terrain/TerrainLayer.h"
#include "world/BaseLayer.h"

#include <algorithm>
#include <cmath>

namespace neuro::routes::protagonist {

namespace {

Vec2 clampToWorld(Vec2 p, Vec2 world_size) {
    p.x = std::clamp(p.x, 0.0f, world_size.x);
    p.y = std::clamp(p.y, 0.0f, world_size.y);
    return p;
}

}  // namespace

TreeLayer::TreeLayer(std::size_t tree_count,
                     float spawn_radius_min,
                     float spawn_radius_max,
                     float trunk_radius,
                     float max_integrity,
                     std::size_t wood_yield,
                     SimTimeSeconds regrow_seconds)
    : tree_count_(tree_count),
      spawn_radius_min_(std::max(1.0f, spawn_radius_min)),
      spawn_radius_max_(std::max(spawn_radius_min_, spawn_radius_max)),
      trunk_radius_(std::max(0.5f, trunk_radius)),
      max_integrity_(std::max(1.0f, max_integrity)),
      wood_yield_(std::max<std::size_t>(1, wood_yield)),
      regrow_seconds_(std::max<SimTimeSeconds>(0.0, regrow_seconds)),
      rng_(1337u) {}

void TreeLayer::onAttach(IWorld& world) {
    base_ = world.getLayer<BaseLayer>();
    objects_ = world.getLayer<ObjectLayer>();
    terrain_ = world.getLayer<TerrainLayer>();
    rng_.seed(static_cast<std::uint32_t>(world.size().x * 17.0f + world.size().y * 31.0f + tree_count_ * 13u));
    seedTrees(world.size());
}

void TreeLayer::tick(IWorld& world, float dt) {
    (void)world;
    for (auto& tree : trees_) {
        if (tree.available || tree.regrow_seconds_remaining <= 0.0) {
            continue;
        }
        tree.regrow_seconds_remaining = std::max<SimTimeSeconds>(0.0, tree.regrow_seconds_remaining - static_cast<SimTimeSeconds>(dt));
        if (tree.regrow_seconds_remaining <= 0.0) {
            tree.available = true;
            tree.integrity = tree.max_integrity;
        }
    }
}

std::optional<TreeResource> TreeLayer::nearestTree(Vec2 position) const {
    const TreeResource* best = nullptr;
    float best_dist_sq = 0.0f;
    for (const auto& tree : trees_) {
        if (!tree.available) {
            continue;
        }
        const float dist_sq = Vec2::distanceSquared(tree.position, position);
        if (best == nullptr || dist_sq < best_dist_sq) {
            best = &tree;
            best_dist_sq = dist_sq;
        }
    }
    return best == nullptr ? std::nullopt : std::optional<TreeResource>(*best);
}

bool TreeLayer::inChopRange(Vec2 position, float chop_radius) const {
    for (const auto& tree : trees_) {
        if (!tree.available) {
            continue;
        }
        const float reach = tree.trunk_radius + chop_radius;
        if (Vec2::distanceSquared(tree.position, position) <= reach * reach) {
            return true;
        }
    }
    return false;
}

ChopHitResult TreeLayer::chopNearest(Vec2 position, float chop_radius, float damage, IWorld& world) {
    (void)world;
    TreeResource* best = nullptr;
    float best_dist_sq = 0.0f;
    for (auto& tree : trees_) {
        if (!tree.available) {
            continue;
        }
        const float reach = tree.trunk_radius + chop_radius;
        const float dist_sq = Vec2::distanceSquared(tree.position, position);
        if (dist_sq > reach * reach) {
            continue;
        }
        if (best == nullptr || dist_sq < best_dist_sq) {
            best = &tree;
            best_dist_sq = dist_sq;
        }
    }
    if (best == nullptr) {
        return ChopHitResult{};
    }

    best->integrity = std::max(0.0f, best->integrity - std::max(0.0f, damage));

    ChopHitResult result;
    result.tree_id = best->id;
    result.tree_pos = best->position;
    result.remaining_integrity = best->integrity;
    result.wood_yield = best->wood_yield;

    if (best->integrity > 0.0f) {
        return result;
    }

    best->available = false;
    best->regrow_seconds_remaining = regrow_seconds_;
    ++harvested_count_;
    result.felled = true;

    if (objects_ != nullptr) {
        std::uniform_real_distribution<float> angle_dist(0.0f, 6.28318530718f);
        std::uniform_real_distribution<float> radius_dist(0.6f, 1.8f);
        for (std::size_t i = 0; i < best->wood_yield; ++i) {
            const float angle = angle_dist(rng_);
            const float radius = radius_dist(rng_);
            const Vec2 offset{std::cos(angle) * radius, std::sin(angle) * radius};
            objects_->objects().push_back(MovableObject{
                objects_->nextId(),
                best->position + offset,
                Vec2{0.0f, 0.0f},
                0.8f,
                ObjectType::Stick,
                false,
                false,
                std::size_t(1)
            });
        }
    }
    return result;
}

std::size_t TreeLayer::availableTreeCount() const {
    std::size_t count = 0;
    for (const auto& tree : trees_) {
        count += tree.available ? 1u : 0u;
    }
    return count;
}

void TreeLayer::seedTrees(Vec2 world_size) {
    trees_.clear();
    if (tree_count_ == 0) {
        return;
    }

    std::uniform_real_distribution<float> angle_dist(0.0f, 6.28318530718f);
    std::uniform_real_distribution<float> radial_dist(spawn_radius_min_, spawn_radius_max_);
    std::uniform_real_distribution<float> x_dist(0.0f, world_size.x);
    std::uniform_real_distribution<float> y_dist(0.0f, world_size.y);

    auto clearOfBase = [&](Vec2 p) {
        return base_ == nullptr || (!base_->inStockpile(p) && !base_->inNest(p));
    };

    trees_.reserve(tree_count_);

    // Biome-driven placement: when a terrain layer is present, scatter trees
    // across the WHOLE map weighted by forest suitability (rejection sampling),
    // so trees follow the ecology instead of ringing the base. Falls back to
    // the legacy radial band when there is no terrain (smoke / unit worlds).
    if (terrain_ != nullptr && terrain_->biomeMap().valid()) {
        std::uniform_real_distribution<float> accept_dist(0.0f, 1.0f);
        for (std::size_t i = 0; i < tree_count_; ++i) {
            Vec2 candidate{x_dist(rng_), y_dist(rng_)};
            for (std::size_t attempt = 0; attempt < 64; ++attempt) {
                candidate = Vec2{x_dist(rng_), y_dist(rng_)};
                const float w = terrain_->biomeMap().treeSuitability(candidate.x, candidate.y);
                if (accept_dist(rng_) <= w && clearOfBase(candidate)) {
                    break;
                }
            }
            trees_.push_back(TreeResource{i + 1u, candidate, max_integrity_, max_integrity_, trunk_radius_, wood_yield_, true, 0u});
        }
        return;
    }

    const Vec2 anchor = base_ != nullptr ? base_->baseCenter() : Vec2{x_dist(rng_), y_dist(rng_)};
    for (std::size_t i = 0; i < tree_count_; ++i) {
        Vec2 candidate = anchor;
        for (std::size_t attempt = 0; attempt < 24; ++attempt) {
            const float angle = angle_dist(rng_);
            const float radius = radial_dist(rng_);
            candidate = clampToWorld(anchor + Vec2{std::cos(angle) * radius, std::sin(angle) * radius}, world_size);
            if (clearOfBase(candidate)) {
                break;
            }
        }
        trees_.push_back(TreeResource{i + 1u, candidate, max_integrity_, max_integrity_, trunk_radius_, wood_yield_, true, 0u});
    }
}

}  // namespace neuro::routes::protagonist
