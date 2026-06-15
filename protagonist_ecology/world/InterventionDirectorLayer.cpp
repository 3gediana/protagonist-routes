#include "world/InterventionDirectorLayer.h"

#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"
#include "core/world/layers/object/ObjectLayer.h"
#include "world/BaseLayer.h"
#include "world/CampfireLayer.h"
#include "world/WorksiteLayer.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>
#include <utility>

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

InterventionDirectorLayer::InterventionDirectorLayer(float initial_budget,
                                                     float max_budget,
                                                     float recharge_per_second,
                                                     float stress_decay_per_second,
                                                     float max_stress,
                                                     SimTimeSeconds cooldown_seconds,
                                                     std::uint32_t seed)
    : budget_(initial_budget),
      max_budget_(max_budget),
      recharge_per_second_(recharge_per_second),
      stress_decay_per_second_(stress_decay_per_second),
      max_stress_(max_stress),
      cooldown_seconds_(std::max<SimTimeSeconds>(0.0, cooldown_seconds)),
      rng_(seed == 0 ? std::random_device{}() : seed) {}

void InterventionDirectorLayer::onAttach(IWorld& world) {
    agents_ = world.getLayer<AgentLayer>();
    campfire_ = world.getLayer<CampfireLayer>();
    worksite_ = world.getLayer<WorksiteLayer>();
    objects_ = world.getLayer<ObjectLayer>();
    base_ = world.getLayer<BaseLayer>();
}

void InterventionDirectorLayer::tick(IWorld& world, float dt) {
    budget_ = std::min(max_budget_, budget_ + recharge_per_second_ * dt);
    stress_ = std::max(0.0f, stress_ - stress_decay_per_second_ * dt);

    if (pending_.empty()) {
        return;
    }

    if (last_execution_time_seconds_ >= 0.0
        && world.currentTimeSeconds() < last_execution_time_seconds_ + cooldown_seconds_) {
        return;
    }

    const Request request = pending_.front();
    if (!canExecute(request, world)) {
        return;
    }

    if (execute(request, world)) {
        pending_.pop_front();
        budget_ = std::max(0.0f, budget_ - request.cost);
        stress_ = std::min(max_stress_, stress_ + request.stress);
        last_execution_time_seconds_ = world.currentTimeSeconds();
        world.eventBus().publish(events::Event{events::EventType::InterventionTriggered,
                                               events::InterventionTriggered{request.source, "director"},
                                               world.currentTick(),
                                               world.currentTimeSeconds()});
    }
}

void InterventionDirectorLayer::submit(Request request) {
    pending_.push_back(std::move(request));
}

bool InterventionDirectorLayer::canExecute(const Request& request, const IWorld& world) const {
    (void)world;
    if (budget_ < request.cost) {
        return false;
    }
    if (stress_ + request.stress > max_stress_) {
        return false;
    }

    if (agents_ != nullptr) {
        const std::size_t living = agents_->livingCount();
        if (living <= 2 && request.stress > 0.2f) {
            return false;
        }
    }

    switch (request.kind) {
        case RequestKind::IgniteCampfire:
            return campfire_ != nullptr;
        case RequestKind::ActivateWorksite:
            return worksite_ != nullptr;
        case RequestKind::ScatterStone:
            return objects_ != nullptr && base_ != nullptr;
        case RequestKind::ScatterHeavyStone:
            return objects_ != nullptr && base_ != nullptr;
    }

    return false;
}

bool InterventionDirectorLayer::execute(const Request& request, IWorld& world) {
    switch (request.kind) {
        case RequestKind::IgniteCampfire:
            return campfire_ != nullptr && campfire_->igniteNextDormant();
        case RequestKind::ActivateWorksite:
            return worksite_ != nullptr && worksite_->activateNextDormant();
        case RequestKind::ScatterStone: {
            if (objects_ == nullptr || base_ == nullptr) {
                return false;
            }

            std::vector<MovableObject*> candidates;
            candidates.reserve(objects_->objects().size());
            for (auto& object : objects_->objects()) {
                if (object.carried || base_->inStockpile(object.pos) || object.type != ObjectType::Stick) {
                    continue;
                }
                const float distance = (object.pos - base_->nestCenter()).length();
                if (distance < 15.0f) {
                    candidates.insert(candidates.begin(), &object);
                } else {
                    candidates.push_back(&object);
                }
            }
            if (candidates.empty()) {
                return false;
            }

            auto* object = candidates.front();

            Vec2 spawn_position{};
            bool anchored_to_worksite = false;
            if (worksite_ != nullptr) {
                for (const auto& site : worksite_->sites()) {
                    if (!site.active || site.completed || site.stored_stones >= site.required_stones) {
                        continue;
                    }
                    std::uniform_real_distribution<float> angle_dist(0.0f, 6.28318530718f);
                    std::uniform_real_distribution<float> radius_dist(site.radius * 0.2f, std::max(site.radius * 0.75f, site.radius * 0.2f + 0.5f));
                    const float angle = angle_dist(rng_);
                    const float radius = radius_dist(rng_);
                    spawn_position = site.position + Vec2{std::cos(angle) * radius, std::sin(angle) * radius};
                    anchored_to_worksite = true;
                    break;
                }
            }
            if (!anchored_to_worksite) {
                const Vec2 trail_start = base_->nestCenter();
                const Vec2 trail_end = base_->stockpileCenter();
                const Vec2 trail = trail_end - trail_start;
                const float trail_len = trail.length();
                Vec2 trail_normal{0.0f, 1.0f};
                if (trail_len > 0.0001f) {
                    trail_normal = Vec2{-trail.y / trail_len, trail.x / trail_len};
                }
                std::uniform_real_distribution<float> progress_dist(0.35f, 0.88f);
                std::uniform_real_distribution<float> jitter_dist(-3.5f, 3.5f);
                spawn_position = trail_start + trail * progress_dist(rng_) + trail_normal * jitter_dist(rng_);
            }
            object->type = ObjectType::Stone;
            object->heavy = false;
            object->mass = 1.6f;
            object->carriers_required = 1;
            object->carried = false;
            object->velocity = kZeroVec2;
            object->pos = keepOutsideStockpile(*base_, spawn_position, world.size());
            return true;
        }
        case RequestKind::ScatterHeavyStone: {
            if (objects_ == nullptr || base_ == nullptr) {
                return false;
            }

            std::vector<MovableObject*> candidates;
            candidates.reserve(objects_->objects().size());
            for (auto& object : objects_->objects()) {
                if (object.carried || base_->inStockpile(object.pos) || (object.type == ObjectType::Stone && object.heavy)) {
                    continue;
                }
                candidates.push_back(&object);
            }
            if (candidates.empty()) {
                return false;
            }

            std::uniform_int_distribution<std::size_t> pick_dist(0, candidates.size() - 1);
            auto* object = candidates[pick_dist(rng_)];

            const bool heavy = request.kind == RequestKind::ScatterHeavyStone;
            Vec2 spawn_position{};
            if (heavy) {
                Vec2 anchor = base_->stockpileCenter();
                float anchor_radius = 10.0f;
                if (worksite_ != nullptr) {
                    for (const auto& site : worksite_->sites()) {
                        if (!site.completed && !site.active) {
                            continue;
                        }
                        anchor = site.position;
                        anchor_radius = site.radius;
                        break;
                    }
                }
                Vec2 corridor_dir = anchor - base_->stockpileCenter();
                float corridor_len = corridor_dir.length();
                if (corridor_len <= 0.0001f) {
                    corridor_dir = base_->stockpileCenter() - base_->baseCenter();
                    corridor_len = corridor_dir.length();
                }
                if (corridor_len <= 0.0001f) {
                    corridor_dir = Vec2{1.0f, 0.0f};
                    corridor_len = 1.0f;
                 }
                 corridor_dir /= corridor_len;
                 const Vec2 corridor_normal{-corridor_dir.y, corridor_dir.x};
                 std::uniform_real_distribution<float> forward_dist(anchor_radius + 0.75f, anchor_radius + 2.5f);
                 std::uniform_real_distribution<float> lateral_dist(-2.0f, 2.0f);
                 spawn_position = anchor + corridor_dir * forward_dist(rng_) + corridor_normal * lateral_dist(rng_);
             } else {
                 const Vec2 trail_start = base_->nestCenter();
                 const Vec2 trail_end = base_->stockpileCenter();
                const Vec2 trail = trail_end - trail_start;
                const float trail_len = trail.length();
                Vec2 trail_normal{0.0f, 1.0f};
                if (trail_len > 0.0001f) {
                    trail_normal = Vec2{-trail.y / trail_len, trail.x / trail_len};
                }
                std::uniform_real_distribution<float> progress_dist(0.35f, 0.88f);
                std::uniform_real_distribution<float> jitter_dist(-3.5f, 3.5f);
                spawn_position = trail_start + trail * progress_dist(rng_) + trail_normal * jitter_dist(rng_);
            }
            object->type = ObjectType::Stone;
            object->heavy = heavy;
            object->mass = object->heavy ? 5.0f : 1.6f;
            object->carriers_required = object->heavy ? std::size_t(2) : std::size_t(1);
            object->carried = false;
            object->velocity = kZeroVec2;
            object->pos = keepOutsideStockpile(*base_, spawn_position, world.size());
            return true;
        }
    }

    return false;
}

}  // namespace neuro::routes::protagonist
