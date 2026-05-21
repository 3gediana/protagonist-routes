#include "runtime/ProtagonistWorldFactory.h"

#include "core/agent/Agent.h"
#include "core/world/World2D.h"
#include "core/world/layers/AgentLayer.h"
#include "core/world/layers/DiseaseLayer.h"
#include "core/world/layers/object/ObjectLayer.h"
#include "core/world/layers/ResourceLayer.h"
#include "core/world/layers/SeasonLayer.h"
#include "core/world/layers/signal/SignalLayer.h"
#include "core/world/layers/terrain/TerrainLayer.h"
#include "core/world/layers/TimeOfDayLayer.h"
#include "world/BaseLayer.h"
#include "world/CampfireLayer.h"
#include "world/InterventionDirectorLayer.h"
#include "world/MemoryTrialLayer.h"
#include "world/NurseryLogisticsLayer.h"
#include "world/NurseryProgressionLayer.h"
#include "world/NurserySupplyLayer.h"
#include "world/SandboxStatusLayer.h"
#include "world/SurvivalStatusLayer.h"
#include "world/TreeLayer.h"
#include "world/WaterLayer.h"
#include "world/WorksiteLayer.h"

#include <algorithm>
#include <cmath>
#include <random>

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

void seedRouteObjects(ObjectLayer& objects,
                      const BaseLayer& base,
                      Vec2 world_size,
                      std::uint32_t seed,
                      bool live_role) {
    std::mt19937 rng(seed == 0 ? std::random_device{}() : seed);
    std::uniform_real_distribution<float> angle_dist(0.0f, 6.28318530718f);
    std::uniform_real_distribution<float> starter_radius_dist(1.0f, std::max(1.5f, base.nestRadius() * 0.55f));
    std::uniform_real_distribution<float> trail_progress_dist(0.18f, 0.78f);
    std::uniform_real_distribution<float> trail_jitter_dist(-2.5f, 2.5f);
    std::uniform_real_distribution<float> outer_radius_dist(base.baseRadius() * 1.0f, base.baseRadius() * 1.85f);
    std::uniform_real_distribution<float> live_inner_radius_dist(base.baseRadius() * 0.35f, base.baseRadius() * 0.95f);

    auto& items = objects.objects();
    const std::size_t starter_count = std::min<std::size_t>(items.size(), std::max<std::size_t>(6, items.size() / 4));
    const std::size_t trail_count = std::min<std::size_t>(items.size() - starter_count, std::max<std::size_t>(8, items.size() / 3));
    const std::size_t live_starter_stone_count = live_role ? std::min<std::size_t>(starter_count / 2, std::size_t(4)) : 0u;
    const std::size_t live_stone_count = live_role ? std::min<std::size_t>(items.size() / 3, std::max<std::size_t>(6, items.size() / 5)) : 0u;
    const Vec2 trail_start = base.nestCenter();
    const Vec2 trail_end = base.stockpileCenter();
    const Vec2 trail_dir = trail_end - trail_start;
    const float trail_len = trail_dir.length();
    Vec2 trail_normal{0.0f, 1.0f};
    if (trail_len > 0.0001f) {
        trail_normal = Vec2{-trail_dir.y / trail_len, trail_dir.x / trail_len};
    }
    for (std::size_t i = 0; i < items.size(); ++i) {
        auto& object = items[i];
        Vec2 spawn_position{};
        const bool live_starter_stone = live_role && i < live_starter_stone_count;
        const bool live_stone = live_role && i >= starter_count && i < starter_count + live_stone_count;
        if (i < starter_count) {
            const float angle = angle_dist(rng);
            const float radius = starter_radius_dist(rng);
            spawn_position = base.nestCenter() + Vec2{std::cos(angle) * radius, std::sin(angle) * radius};
            object.mass = live_starter_stone ? 1.35f : 0.75f;
        } else if (live_stone) {
            const float angle = angle_dist(rng);
            const float radius = live_inner_radius_dist(rng);
            spawn_position = base.baseCenter() + Vec2{std::cos(angle) * radius, std::sin(angle) * radius};
            object.mass = 1.45f;
        } else if (i < starter_count + trail_count) {
            const float progress = trail_progress_dist(rng);
            const float jitter = trail_jitter_dist(rng);
            spawn_position = trail_start + trail_dir * progress + trail_normal * jitter;
            object.mass = 0.9f;
        } else {
            const float angle = angle_dist(rng);
            const float radius = outer_radius_dist(rng);
            spawn_position = base.baseCenter() + Vec2{std::cos(angle) * radius, std::sin(angle) * radius};
            object.mass = 1.0f;
        }
        object.pos = keepOutsideStockpile(base, spawn_position, world_size);
        object.velocity = kZeroVec2;
        object.carried = false;
        object.type = (live_starter_stone || live_stone) ? ObjectType::Stone : ObjectType::Stick;
        object.heavy = false;
        object.carriers_required = std::size_t(1);
    }
}

}  // namespace

std::unique_ptr<World2D> createBootstrapWorld(const ProtagonistEcologyConfig& config) {
    auto world = std::make_unique<World2D>(WorldId{1}, config.world_size);
    const bool live_role = config.isLiveRole();
    const bool enable_curriculum_supply = !live_role && config.base.enabled && config.objects.enabled && config.nursery_supply.enabled;
    const bool enable_curriculum_progression = !live_role && config.base.enabled && config.objects.enabled && config.progression.enabled;

    world->addLayer(std::make_unique<ResourceLayer>(
        config.resources.count,
        config.resources.amount,
        config.resources.respawn_seconds,
        config.random_seed + 1,
        config.resources.dynamic,
        config.resources.regen_rate,
        config.resources.terrain_adjustment_enabled));
    world->addLayer(std::make_unique<AgentLayer>(0.0f, 0.0f, config.compute.agent_decision_threads));
    if (config.memory_trial.enabled) {
        world->addLayer(std::make_unique<MemoryTrialLayer>(config.memory_trial, config.random_seed + 907));
    }

    if (config.terrain_enabled) {
        world->addLayer(std::make_unique<TerrainLayer>(config.world_size, config.terrain));
    }

    if (config.base.enabled) {
        world->addLayer(std::make_unique<BaseLayer>(
            config.base.center,
            config.base.radius,
            config.base.stockpile_radius,
            config.base.nest_radius,
            config.base.stockpile_offset,
            config.base.nest_offset));
    }

    if (config.objects.enabled) {
        auto objects = std::make_unique<ObjectLayer>(config.world_size, config.objects.count, config.objects.damping);
        if (config.base.enabled) {
            if (const auto* base_layer = world->getLayer<BaseLayer>(); base_layer != nullptr) {
                seedRouteObjects(*objects, *base_layer, config.world_size, config.random_seed + 2, live_role);
            }
        }
        world->addLayer(std::move(objects));
    }

    if (config.base.enabled && config.objects.enabled && config.tree.enabled) {
        world->addLayer(std::make_unique<TreeLayer>(
            config.tree.count,
            config.tree.spawn_radius_min,
            config.tree.spawn_radius_max,
            config.tree.trunk_radius,
            config.tree.max_integrity,
            config.tree.wood_yield,
            config.tree.regrow_seconds));
    }

    if (config.signal.enabled) {
        world->addLayer(std::make_unique<SignalLayer>(
            config.world_size,
            config.signal.grid_resolution,
            config.signal.channels,
            config.signal.decay_rate,
            config.signal.diffusion_rate));
    }

    if (config.base.enabled && config.campfire.enabled) {
        world->addLayer(std::make_unique<CampfireLayer>(
            config.campfire.count,
            config.campfire.spawn_radius,
            config.campfire.warmth_radius,
            config.campfire.danger_radius,
            config.campfire.warmth_energy_bonus,
            config.campfire.danger_damage,
            config.random_seed + 901));
    }

    if (config.base.enabled && config.water.enabled) {
        world->addLayer(std::make_unique<WaterLayer>(
            config.water.source_count,
            config.water.spawn_radius_min,
            config.water.spawn_radius_max,
            config.water.source_radius,
            config.water.drink_radius,
            config.water.drink_rate,
            config.random_seed + 906));
    }

    if (config.survival.enabled) {
        world->addLayer(std::make_unique<SurvivalStatusLayer>(
            config.survival.max_health,
            config.survival.max_hydration,
            config.survival.initial_health_ratio,
            config.survival.initial_hydration_ratio,
            config.survival.hydration_decay_per_second,
            config.survival.dehydration_threshold_ratio,
            config.survival.dehydration_damage_per_second,
            config.survival.fire_health_damage_per_second,
            config.survival.warmth_recovery_per_second,
            config.survival.nest_recovery_per_second,
            config.survival.injured_threshold_ratio));
    }

    if (config.base.enabled && config.worksite.enabled) {
        world->addLayer(std::make_unique<WorksiteLayer>(
            config.worksite.count,
            config.worksite.spawn_radius,
            config.worksite.interaction_radius,
            config.worksite.required_sticks,
            config.worksite.required_stones,
            config.worksite.assembly_rate,
            config.worksite.shelter_energy_bonus,
            config.random_seed + 903));
    }

    if (config.intervention.enabled) {
        world->addLayer(std::make_unique<InterventionDirectorLayer>(
            config.intervention.initial_budget,
            config.intervention.max_budget,
            config.intervention.recharge_per_second,
            config.intervention.stress_decay_per_second,
            config.intervention.max_stress,
            config.intervention.cooldown_seconds,
            config.random_seed + 904));
    }

    if (enable_curriculum_supply) {
        world->addLayer(std::make_unique<NurserySupplyLayer>(
            config.nursery_supply.min_nearby_sticks,
            config.nursery_supply.replenish_batch,
            config.nursery_supply.near_radius,
            config.nursery_supply.spawn_radius,
            config.nursery_supply.cooldown_seconds,
            config.random_seed + 905));
    }

    if (config.base.enabled && config.objects.enabled && config.nursery.enabled) {
        world->addLayer(std::make_unique<NurseryLogisticsLayer>(
            config.nursery.deposit_energy_bonus,
            config.nursery.deposit_fitness_bonus,
            config.nursery.stockpile_snap_radius,
            config.random_seed + 900));
    }

    if (enable_curriculum_progression) {
        world->addLayer(std::make_unique<NurseryProgressionLayer>(
            config.progression.first_fire_stick_threshold,
            config.progression.worksite_unlock_deposit_threshold,
            config.progression.stone_unlock_deposit_threshold,
            config.progression.heavy_unlock_deposit_threshold,
            config.progression.second_fire_deposit_threshold,
            config.progression.worksite_unlock_stone_threshold,
            config.progression.heavy_unlock_stone_threshold,
            config.progression.second_fire_stone_threshold,
            config.progression.stone_unlock_count,
            config.progression.heavy_unlock_count,
            config.progression.unlock_radius_min,
            config.progression.unlock_radius_max,
            config.progression.event_min_interval_seconds,
            config.progression.event_max_interval_seconds,
            config.random_seed + 902));
    }

    if (live_role) {
        if (auto* campfire = world->getLayer<CampfireLayer>(); campfire != nullptr) {
            campfire->igniteNextDormant();
        }
        if (auto* worksite = world->getLayer<WorksiteLayer>(); worksite != nullptr) {
            worksite->activateNextDormant();
        }
    }

    if (config.day_night.enabled) {
        world->addLayer(std::make_unique<TimeOfDayLayer>(
            config.day_night.day_length_seconds,
            config.day_night.initial_time));
    }

    if (config.season.enabled) {
        world->addLayer(std::make_unique<SeasonLayer>(
            config.season.season_length_seconds,
            config.season.start_time_seconds));
    }

    if (config.disease.enabled) {
        world->addLayer(std::make_unique<DiseaseLayer>(
            config.disease.infection_radius,
            config.disease.infection_prob,
            config.disease.infection_duration,
            config.disease.death_prob,
            config.disease.initial_infection_rate,
            config.random_seed + 6000));
    }

    world->addLayer(std::make_unique<SandboxStatusLayer>(20));

    return world;
}

}  // namespace neuro::routes::protagonist
