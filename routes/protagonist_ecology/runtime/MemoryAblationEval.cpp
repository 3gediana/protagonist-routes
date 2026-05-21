#include "runtime/MemoryAblationEval.h"

#include <algorithm>
#include <array>
#include <string>

namespace neuro::routes::protagonist {

namespace {

const std::array<MemoryAblationTaskKind, 4> kMemoryAblationTasks{
    MemoryAblationTaskKind::WaterReturn,
    MemoryAblationTaskKind::DangerAvoidance,
    MemoryAblationTaskKind::WorksiteReturn,
    MemoryAblationTaskKind::SignalResponse,
};

const std::array<MemoryAblationVariant, 5> kMemoryAblationVariants{{
    {"baseline", true, true, true, true},
    {"no_spatial", false, true, true, true},
    {"no_episodic", true, false, true, true},
    {"no_social", true, true, false, true},
    {"no_goal", true, true, true, false},
}};

}  // namespace

MemoryAblationSettings loadMemoryAblationSettings(const config::Config& config) {
    MemoryAblationSettings out;
    out.enabled = config.getOr<bool>("memory_ablation.enabled", out.enabled);
    out.episodes_per_task = std::max<std::size_t>(
        1,
        config.getOr<std::size_t>("memory_ablation.episodes_per_task", out.episodes_per_task));
    out.trace_enabled = config.getOr<bool>("memory_ablation.trace_enabled", out.trace_enabled);
    return out;
}

std::string_view memoryAblationTaskName(MemoryAblationTaskKind task) {
    switch (task) {
        case MemoryAblationTaskKind::WaterReturn: return "water_return";
        case MemoryAblationTaskKind::DangerAvoidance: return "danger_avoidance";
        case MemoryAblationTaskKind::WorksiteReturn: return "worksite_return";
        case MemoryAblationTaskKind::SignalResponse: return "signal_response";
    }
    return "unknown";
}

const std::array<MemoryAblationTaskKind, 4>& memoryAblationTasks() {
    return kMemoryAblationTasks;
}

const std::array<MemoryAblationVariant, 5>& memoryAblationVariants() {
    return kMemoryAblationVariants;
}

ProtagonistEcologyConfig configureMemoryAblationTask(const ProtagonistEcologyConfig& base,
                                                     MemoryAblationTaskKind task) {
    auto route = base;
    route.scenario_name = "memory_ablation_" + std::string(memoryAblationTaskName(task));
    route.progression.enabled = false;
    route.nursery_supply.enabled = false;
    route.intervention.enabled = false;
    route.day_night.enabled = false;
    route.season.enabled = false;
    route.disease.enabled = false;
    route.resources.dynamic = false;
    route.resources.regen_rate = 0.0f;
    route.resources.terrain_adjustment_enabled = false;
    route.background.slots.clear();
    route.background.ambient_count = 0;

    switch (task) {
        case MemoryAblationTaskKind::WaterReturn:
            route.world_role = "eval";
            route.world_size = Vec2{120.0f, 120.0f};
            route.base.center = Vec2{44.0f, 60.0f};
            route.signal.protagonist_emit_enabled = false;
            route.protagonist.hunt_enabled = false;
            route.protagonist.goal_action_assist_enabled = true;
            route.background.forager_count = 1;
            route.background.predator_count = 0;
            route.resources.count = 0;
            route.resources.amount = std::max(route.resources.amount, 18.0f);
            route.water.source_count = 1;
            route.water.spawn_radius_min = 26.0f;
            route.water.spawn_radius_max = 26.0f;
            route.water.direction_requires_visibility = true;
            route.survival.initial_hydration_ratio = std::min(route.survival.initial_hydration_ratio, 0.30f);
            route.survival.hydration_decay_per_second = std::max(route.survival.hydration_decay_per_second, 0.075f);
            route.objects.count = 10;
            route.tree.count = 1;
            route.campfire.count = 0;
            route.worksite.count = 0;
            break;

        case MemoryAblationTaskKind::DangerAvoidance:
            route.world_role = "eval";
            route.world_size = Vec2{96.0f, 96.0f};
            route.base.center = Vec2{36.0f, 48.0f};
            route.signal.protagonist_emit_enabled = false;
            route.background.forager_count = 0;
            route.background.predator_count = 6;
            route.protagonist.predator_warn_enabled = false;
            route.resources.count = 18;
            route.water.source_count = 1;
            route.survival.initial_health_ratio = std::max(route.survival.initial_health_ratio, 0.90f);
            route.survival.initial_hydration_ratio = std::max(route.survival.initial_hydration_ratio, 0.65f);
            route.objects.count = 8;
            route.tree.count = 1;
            route.campfire.count = 0;
            route.worksite.count = 0;
            break;

        case MemoryAblationTaskKind::WorksiteReturn:
            route.world_role = "live";
            route.world_size = Vec2{120.0f, 120.0f};
            route.base.center = Vec2{44.0f, 60.0f};
            route.nursery.enabled = false;
            route.signal.protagonist_emit_enabled = false;
            route.protagonist.hunt_enabled = false;
            route.protagonist.goal_action_assist_enabled = true;
            route.background.forager_count = 1;
            route.background.predator_count = 0;
            route.resources.count = 10;
            route.water.source_count = 1;
            route.objects.count = 24;
            route.tree.count = 2;
            route.campfire.count = 0;
            route.worksite.count = 1;
            route.worksite.spawn_radius = 10.0f;
            route.worksite.required_sticks = 2;
            route.worksite.required_stones = 1;
            route.worksite.assembly_rate = std::max(route.worksite.assembly_rate, 0.25f);
            route.worksite.direction_requires_visibility = true;
            route.survival.initial_hydration_ratio = std::max(route.survival.initial_hydration_ratio, 0.75f);
            break;

        case MemoryAblationTaskKind::SignalResponse:
            route.world_role = "eval";
            route.world_size = Vec2{120.0f, 120.0f};
            route.base.center = Vec2{44.0f, 60.0f};
            route.base.radius = 12.0f;
            route.base.stockpile_radius = 8.0f;
            route.base.nest_radius = 4.0f;
            route.background.forager_count = 1;
            route.background.predator_count = 0;
            route.resources.count = 0;
            route.resources.amount = std::max(route.resources.amount, 22.0f);
            route.water.source_count = 1;
            route.signal.protagonist_emit_enabled = false;
            route.objects.count = 10;
            route.tree.count = 1;
            route.campfire.count = 0;
            route.worksite.count = 0;
            route.survival.initial_hydration_ratio = std::max(route.survival.initial_hydration_ratio, 0.70f);
            break;
    }

    return route;
}

}  // namespace neuro::routes::protagonist
