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

// Stone Bootstrap commit 1: caller (ProtagonistEcologyRuntime) hands
// us the archetype's persisted unlock state right after onAttach.
// We OR-merge into our own flags so a stale-false in `state` never
// silently overrides a true we somehow already had set.
//
// Stone Bootstrap commit 4 (bug fix): persisted flags ONLY record
// "this archetype has earned the unlock"; they do NOT carry the
// physical world side-effects (stones on the ground, ignited fires,
// activated worksites). Every new episode bootstraps a fresh world
// via createBootstrapWorld with all sticks (zero stones), all
// dormant campfires/worksites. Pre-fix behaviour: if seed carried
// stone_unlocked=true, the layer's `if (!stone_unlocked_ ...)` gate
// in dispatchRandomPendingEvent short-circuits and ScatterStone
// never fires again - the agent inherits "stone_unlocked=true" but
// lives in a stone-free world. Graduate AB 60-gen showed
// deposits_stone=0 for every single generation as a direct result.
//
// For each flag that transitions false → true via seeding, we
// replay the side-effect that dispatchRandomPendingEvent would have
// fired when that unlock originally happened. Replay is immediate
// (during seed, before the new episode's first tick) so stones are
// scattered into the world during the initial intervention budget,
// well before the agent finishes its first deposits. Each replay is
// guarded by a null-check on the collaborating layer pointer so
// existing unit tests that construct a bare NurseryProgressionLayer
// without attaching it to a world keep working.
void NurseryProgressionLayer::seedFromProgressionState(const ArchetypeProgressionState& state) {
    const bool prev_first_fire   = first_fire_ignited_;
    const bool prev_worksite     = worksite_unlocked_;
    const bool prev_stone        = stone_unlocked_;
    const bool prev_heavy        = heavy_unlocked_;
    const bool prev_second_fire  = second_fire_ignited_;

    first_fire_ignited_   = first_fire_ignited_   || state.first_fire_ignited;
    worksite_unlocked_    = worksite_unlocked_    || state.worksite_unlocked;
    stone_unlocked_       = stone_unlocked_       || state.stone_unlocked;
    heavy_unlocked_       = heavy_unlocked_       || state.heavy_unlocked;
    second_fire_ignited_  = second_fire_ignited_  || state.second_fire_ignited;

    if (!prev_first_fire && first_fire_ignited_ && campfire_ != nullptr) {
        campfire_->igniteNextDormant();
    }
    if (!prev_worksite && worksite_unlocked_ && worksite_ != nullptr) {
        worksite_->activateNextDormant();
        if (director_ != nullptr) {
            director_->submit({InterventionDirectorLayer::RequestKind::ScatterStone,
                               "progression.worksite_support.replay", 0.08f, 0.03f});
        }
    }
    if (!prev_stone && stone_unlocked_ && director_ != nullptr) {
        for (std::size_t i = 0; i < stone_unlock_count_; ++i) {
            director_->submit({InterventionDirectorLayer::RequestKind::ScatterStone,
                               "progression.stone.replay", 0.08f, 0.03f});
        }
    }
    if (!prev_heavy && heavy_unlocked_ && director_ != nullptr) {
        for (std::size_t i = 0; i < heavy_unlock_count_; ++i) {
            director_->submit({InterventionDirectorLayer::RequestKind::ScatterHeavyStone,
                               "progression.heavy.replay", 0.14f, 0.07f});
        }
    }
    if (!prev_second_fire && second_fire_ignited_ && campfire_ != nullptr) {
        campfire_->igniteNextDormant();
    }
}

ArchetypeProgressionState NurseryProgressionLayer::progressionState() const {
    return ArchetypeProgressionState{
        first_fire_ignited_,
        worksite_unlocked_,
        stone_unlocked_,
        heavy_unlocked_,
        second_fire_ignited_,
    };
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
