#include "world/NurseryProgressionLayer.h"

#include "core/interfaces/IWorld.h"
#include "core/world/layers/object/ObjectLayer.h"
#include "world/BaseLayer.h"
#include "world/CampfireLayer.h"
#include "world/InterventionDirectorLayer.h"
#include "world/NurseryLogisticsLayer.h"
#include "world/WorksiteLayer.h"

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

}  // namespace

NurseryProgressionLayer::NurseryProgressionLayer(std::size_t first_fire_stick_threshold,
                                                 std::size_t worksite_unlock_deposit_threshold,
                                                 std::size_t stone_unlock_deposit_threshold,
                                                 std::size_t heavy_unlock_deposit_threshold,
                                                 std::size_t second_fire_deposit_threshold,
                                                 std::size_t worksite_unlock_stone_threshold,
                                                 std::size_t heavy_unlock_stone_threshold,
                                                 std::size_t second_fire_stone_threshold,
                                                 std::size_t stone_unlock_count,
                                                 std::size_t heavy_unlock_count,
                                                 float unlock_radius_min,
                                                 float unlock_radius_max,
                                                 SimTimeSeconds event_min_interval_seconds,
                                                 SimTimeSeconds event_max_interval_seconds,
                                                 std::uint32_t seed)
    : first_fire_stick_threshold_(first_fire_stick_threshold),
      worksite_unlock_deposit_threshold_(worksite_unlock_deposit_threshold),
      stone_unlock_deposit_threshold_(stone_unlock_deposit_threshold),
      heavy_unlock_deposit_threshold_(heavy_unlock_deposit_threshold),
      second_fire_deposit_threshold_(second_fire_deposit_threshold),
      worksite_unlock_stone_threshold_(worksite_unlock_stone_threshold),
      heavy_unlock_stone_threshold_(heavy_unlock_stone_threshold),
      second_fire_stone_threshold_(second_fire_stone_threshold),
      stone_unlock_count_(stone_unlock_count),
      heavy_unlock_count_(heavy_unlock_count),
      unlock_radius_min_(unlock_radius_min),
      unlock_radius_max_(std::max(unlock_radius_min, unlock_radius_max)),
      event_min_interval_seconds_(std::max<SimTimeSeconds>(0.0, event_min_interval_seconds)),
      event_max_interval_seconds_(std::max(event_min_interval_seconds_, event_max_interval_seconds)),
      rng_(seed == 0 ? std::random_device{}() : seed),
      event_scheduled_(false),
      next_event_time_seconds_(0.0) {}

void NurseryProgressionLayer::onAttach(IWorld& world) {
    base_ = world.getLayer<BaseLayer>();
    objects_ = world.getLayer<ObjectLayer>();
    campfire_ = world.getLayer<CampfireLayer>();
    director_ = world.getLayer<InterventionDirectorLayer>();
    logistics_ = world.getLayer<NurseryLogisticsLayer>();
    worksite_ = world.getLayer<WorksiteLayer>();
    world_size_ = world.size();
}

void NurseryProgressionLayer::tick(IWorld& world, float dt) {
    (void)world;
    (void)dt;
    if (base_ == nullptr || objects_ == nullptr || logistics_ == nullptr) {
        return;
    }

    if (!event_scheduled_ && hasPendingEvents()) {
        next_event_time_seconds_ = world.currentTimeSeconds() + drawEventDelaySeconds();
        event_scheduled_ = true;
    }

    if (event_scheduled_ && world.currentTimeSeconds() >= next_event_time_seconds_) {
        event_scheduled_ = false;
        dispatchRandomPendingEvent();
        if (hasPendingEvents()) {
            next_event_time_seconds_ = world.currentTimeSeconds() + drawEventDelaySeconds();
            event_scheduled_ = true;
        }
    }
}

bool NurseryProgressionLayer::hasPendingEvents() const {
    const auto stick_stock = logistics_ != nullptr ? logistics_->stockpiledCount(ObjectType::Stick) : 0u;
    const auto deposits = logistics_ != nullptr ? logistics_->totalDeposits() : 0u;
    const auto stone_stock = logistics_ != nullptr ? logistics_->stockpiledCount(ObjectType::Stone) : 0u;
    const auto completed_worksites = worksite_ != nullptr ? worksite_->completedSiteCount() : 0u;
    const bool first_fire_ready = !first_fire_ignited_ && campfire_ != nullptr && stick_stock >= first_fire_stick_threshold_;
    const bool worksite_stone_phase_ready = stone_unlocked_ || stone_stock >= worksite_unlock_stone_threshold_;
    const bool worksite_ready = !worksite_unlocked_ && worksite_ != nullptr && deposits >= worksite_unlock_deposit_threshold_ && worksite_stone_phase_ready;
    const bool stone_ready = !stone_unlocked_ && deposits >= stone_unlock_deposit_threshold_;
    const bool heavy_ready = !heavy_unlocked_ && stone_stock >= heavy_unlock_stone_threshold_ && (deposits >= heavy_unlock_deposit_threshold_ || completed_worksites > 0);
    const bool second_fire_ready = !second_fire_ignited_ && campfire_ != nullptr && deposits >= second_fire_deposit_threshold_ && stone_stock >= second_fire_stone_threshold_;
    return first_fire_ready || worksite_ready || stone_ready || heavy_ready || second_fire_ready;
}

bool NurseryProgressionLayer::dispatchRandomPendingEvent() {
    const auto stick_stock = logistics_ != nullptr ? logistics_->stockpiledCount(ObjectType::Stick) : 0u;
    const auto deposits = logistics_ != nullptr ? logistics_->totalDeposits() : 0u;
    const auto stone_stock = logistics_ != nullptr ? logistics_->stockpiledCount(ObjectType::Stone) : 0u;
    const auto completed_worksites = worksite_ != nullptr ? worksite_->completedSiteCount() : 0u;
    const bool worksite_stone_phase_ready = stone_unlocked_ || stone_stock >= worksite_unlock_stone_threshold_;

    if (!first_fire_ignited_ && campfire_ != nullptr && stick_stock >= first_fire_stick_threshold_) {
        first_fire_ignited_ = campfire_->igniteNextDormant();
        return first_fire_ignited_;
    }

    if (!stone_unlocked_ && deposits >= stone_unlock_deposit_threshold_) {
        if (director_ != nullptr) {
            for (std::size_t i = 0; i < stone_unlock_count_; ++i) {
                director_->submit({InterventionDirectorLayer::RequestKind::ScatterStone, "progression.stone", 0.08f, 0.03f});
            }
        }
        stone_unlocked_ = true;
        return stone_unlocked_;
    }

    if (!worksite_unlocked_ && worksite_ != nullptr && deposits >= worksite_unlock_deposit_threshold_ && worksite_stone_phase_ready) {
        worksite_unlocked_ = worksite_->activateNextDormant();
        if (worksite_unlocked_ && director_ != nullptr) {
            director_->submit({InterventionDirectorLayer::RequestKind::ScatterStone, "progression.worksite_support", 0.08f, 0.03f});
        }
        return worksite_unlocked_;
    }

    if (!heavy_unlocked_ && stone_stock >= heavy_unlock_stone_threshold_ && (deposits >= heavy_unlock_deposit_threshold_ || completed_worksites > 0)) {
        if (director_ != nullptr) {
            for (std::size_t i = 0; i < heavy_unlock_count_; ++i) {
                director_->submit({InterventionDirectorLayer::RequestKind::ScatterHeavyStone, "progression.heavy", 0.14f, 0.07f});
            }
        }
        heavy_unlocked_ = true;
        return heavy_unlocked_;
    }

    if (!second_fire_ignited_ && campfire_ != nullptr && deposits >= second_fire_deposit_threshold_ && stone_stock >= second_fire_stone_threshold_) {
        second_fire_ignited_ = campfire_->igniteNextDormant();
        return second_fire_ignited_;
    }

    return false;
}

SimTimeSeconds NurseryProgressionLayer::drawEventDelaySeconds() {
    std::uniform_real_distribution<SimTimeSeconds> delay_dist(event_min_interval_seconds_, event_max_interval_seconds_);
    return delay_dist(rng_);
}

std::size_t NurseryProgressionLayer::promoteObjects(std::size_t count, bool heavy) {
    if (count == 0 || base_ == nullptr || objects_ == nullptr) {
        return 0;
    }

    std::vector<MovableObject*> candidates;
    candidates.reserve(objects_->objects().size());
    for (auto& object : objects_->objects()) {
        if (object.carried || base_->inStockpile(object.pos)) {
            continue;
        }
        if (heavy) {
            if (object.type == ObjectType::Stone && object.heavy) {
                continue;
            }
        } else if (object.type == ObjectType::Stone) {
            continue;
        }
        candidates.push_back(&object);
    }

    std::sort(candidates.begin(), candidates.end(), [this](const MovableObject* lhs, const MovableObject* rhs) {
        const float dl = Vec2::distanceSquared(lhs->pos, base_->baseCenter());
        const float dr = Vec2::distanceSquared(rhs->pos, base_->baseCenter());
        return dl > dr;
    });

    std::uniform_real_distribution<float> angle_dist(0.0f, 6.28318530718f);
    std::uniform_real_distribution<float> radius_dist(unlock_radius_min_, unlock_radius_max_);
    std::size_t changed = 0;

    for (std::size_t i = 0; i < candidates.size() && changed < count; ++i) {
        auto* object = candidates[i];
        const float angle = angle_dist(rng_);
        const float radius = radius_dist(rng_);
        object->type = ObjectType::Stone;
        object->heavy = heavy;
        object->mass = heavy ? 5.0f : 1.6f;
        object->carriers_required = heavy ? std::size_t(2) : std::size_t(1);
        object->velocity = kZeroVec2;
        object->pos = clampToWorld(base_->baseCenter() + Vec2{std::cos(angle) * radius, std::sin(angle) * radius}, world_size_);
        ++changed;
    }

    return changed;
}

}  // namespace neuro::routes::protagonist
