#include "world/WaterLayer.h"

#include "core/interfaces/IWorld.h"
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

WaterLayer::WaterLayer(std::size_t source_count,
                       float spawn_radius_min,
                       float spawn_radius_max,
                       float source_radius,
                       float drink_radius,
                       float drink_rate,
                       std::uint32_t seed)
    : source_count_(source_count),
      spawn_radius_min_(std::max(1.0f, spawn_radius_min)),
      spawn_radius_max_(std::max(spawn_radius_min_, spawn_radius_max)),
      source_radius_(std::max(1.0f, source_radius)),
      drink_radius_(std::max(0.5f, drink_radius)),
      drink_rate_(std::max(0.0f, drink_rate)),
      rng_(seed == 0 ? std::random_device{}() : seed) {}

void WaterLayer::onAttach(IWorld& world) {
    base_ = world.getLayer<BaseLayer>();
    terrain_ = world.getLayer<TerrainLayer>();
    seedSources(world.size());
}

void WaterLayer::tick(IWorld& world, float dt) {
    (void)world;
    (void)dt;
}

std::optional<WaterSource> WaterLayer::nearestWater(Vec2 position) const {
    if (sources_.empty()) {
        return std::nullopt;
    }

    const WaterSource* best = nullptr;
    float best_dist_sq = 0.0f;
    for (const auto& source : sources_) {
        const float dist_sq = Vec2::distanceSquared(source.position, position);
        if (best == nullptr || dist_sq < best_dist_sq) {
            best = &source;
            best_dist_sq = dist_sq;
        }
    }
    return best == nullptr ? std::nullopt : std::optional<WaterSource>(*best);
}

bool WaterLayer::inDrinkZone(Vec2 position) const {
    const float radius = source_radius_ + drink_radius_;
    for (const auto& source : sources_) {
        if (Vec2::distanceSquared(source.position, position) <= radius * radius) {
            return true;
        }
    }
    return false;
}

void WaterLayer::seedSources(Vec2 world_size) {
    sources_.clear();
    if (source_count_ == 0) {
        return;
    }

    std::uniform_real_distribution<float> angle_dist(0.0f, 6.28318530718f);
    std::uniform_real_distribution<float> radial_dist(spawn_radius_min_, spawn_radius_max_);
    std::uniform_real_distribution<float> x_dist(0.0f, world_size.x);
    std::uniform_real_distribution<float> y_dist(0.0f, world_size.y);

    auto clearOfBase = [&](Vec2 p) {
        return base_ == nullptr || (!base_->inStockpile(p) && !base_->inNest(p));
    };

    sources_.reserve(source_count_);

    // Biome-driven placement: scatter water sources across the WHOLE map
    // weighted toward river/swamp/low-basin ecology (rejection sampling), so
    // water follows the terrain instead of ringing the base. Falls back to the
    // legacy radial band when there is no terrain (smoke / unit worlds).
    if (terrain_ != nullptr && terrain_->biomeMap().valid()) {
        std::uniform_real_distribution<float> accept_dist(0.0f, 1.0f);
        for (std::size_t i = 0; i < source_count_; ++i) {
            Vec2 candidate{x_dist(rng_), y_dist(rng_)};
            for (std::size_t attempt = 0; attempt < 64; ++attempt) {
                candidate = Vec2{x_dist(rng_), y_dist(rng_)};
                const float w = terrain_->biomeMap().waterSuitability(candidate.x, candidate.y);
                if (accept_dist(rng_) <= w && clearOfBase(candidate)) {
                    break;
                }
            }
            sources_.push_back(WaterSource{i + 1u, candidate, source_radius_});
        }
        return;
    }

    const Vec2 anchor = base_ != nullptr ? base_->baseCenter() : Vec2{x_dist(rng_), y_dist(rng_)};
    for (std::size_t i = 0; i < source_count_; ++i) {
        const float angle = angle_dist(rng_);
        const float radius = radial_dist(rng_);
        const Vec2 offset{std::cos(angle) * radius, std::sin(angle) * radius};
        sources_.push_back(WaterSource{i + 1u, clampToWorld(anchor + offset, world_size), source_radius_});
    }
}

}  // namespace neuro::routes::protagonist
