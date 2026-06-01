#include "runtime/MemoryAblationEval.h"

#include <algorithm>
#include <array>
#include <string>

namespace neuro::routes::protagonist {

namespace {

const std::array<MemoryAblationTaskKind, 7> kMemoryAblationTasks{
    MemoryAblationTaskKind::WaterReturn,
    MemoryAblationTaskKind::DangerAvoidance,
    MemoryAblationTaskKind::WorksiteReturn,
    MemoryAblationTaskKind::SignalResponse,
    MemoryAblationTaskKind::CommScoutWorker,
    MemoryAblationTaskKind::CommDangerWarning,
    MemoryAblationTaskKind::CommBuildRequest,
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
        case MemoryAblationTaskKind::CommScoutWorker: return "comm_scout_worker";
        case MemoryAblationTaskKind::CommDangerWarning: return "comm_danger_warning";
        case MemoryAblationTaskKind::CommBuildRequest: return "comm_build_request";
    }
    return "unknown";
}

const std::array<MemoryAblationTaskKind, 7>& memoryAblationTasks() {
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
            // Water spawns just outside WaterPerception sense radius (36) so
            // the agent cannot see it from spawn. Once it explores in the
            // right direction (or wanders close enough), the source enters
            // perception and gets written into SpatialMemory. With spatial
            // writes disabled, the agent only re-finds water by chance / by
            // staying inside the 36u sense bubble around the source.
            route.water.spawn_radius_min = 42.0f;
            route.water.spawn_radius_max = 42.0f;
            route.water.direction_requires_visibility = true;
            // hydration values are ABSOLUTE units against max_hydration=100,
            // not ratios. Start at 65 (above the dehydration threshold of
            // 32) so the agent has time to explore. Decay 0.6/s puts the
            // agent into dehydration around ~55s, leaving room for ~2-3
            // drink/return cycles per 200s episode. With spawn_radius 42 >
            // sense_radius 36 the source is invisible from the nest, so the
            // second time the agent gets thirsty it has to rely on memory.
            route.survival.initial_hydration_ratio = 0.65f;
            route.survival.hydration_decay_per_second = 0.6f;
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
            // SignalResponse ablation now drives the controlled emitter via
            // the flag instead of relying on the legacy scenario_name match
            // in usesControlledSignalEmitter. Position stays deterministic
            // for eval, no food drops (we want to score memory recall, not
            // a free meal).
            route.signal.controlled_emitter_enabled = true;
            route.signal.controlled_emitter_period_seconds = 30.0;
            route.signal.controlled_emitter_randomize_position = false;
            route.signal.controlled_emitter_food_count = 0;
            route.objects.count = 10;
            route.tree.count = 1;
            route.campfire.count = 0;
            route.worksite.count = 0;
            route.survival.initial_hydration_ratio = std::max(route.survival.initial_hydration_ratio, 0.70f);
            break;

        case MemoryAblationTaskKind::CommScoutWorker:
            // A3.S2 - partial observability scout-worker task. The controlled
            // emitter "scout" beacons over a food drop the protagonist
            // "worker" cannot see directly (food count 0 elsewhere). Worker
            // must follow the signal to reach the food. Differs from
            // SignalResponse in that there IS a real reward at the emitter
            // (food drop), so reaching the emitter is grounded in survival
            // not just halo-entry score. This is the strongest minimal
            // probe for "signal -> behavior change".
            route.world_role = "eval";
            route.world_size = Vec2{120.0f, 120.0f};
            route.base.center = Vec2{44.0f, 60.0f};
            route.base.radius = 12.0f;
            route.base.stockpile_radius = 8.0f;
            route.base.nest_radius = 4.0f;
            // Keep at least one background agent so AgentSocialPerception stays
            // in the sensory dim and the loaded protagonist genome's input
            // shape matches the base ablation config. forager_count=1 is the
            // same convention the other 4 ablation tasks use.
            route.background.forager_count = 1;
            route.background.predator_count = 0;
            route.resources.count = 0;
            route.water.source_count = 1;
            route.signal.protagonist_emit_enabled = false;
            route.signal.controlled_emitter_enabled = true;
            route.signal.controlled_emitter_period_seconds = 20.0;  // shorter cadence than SignalResponse
            route.signal.controlled_emitter_randomize_position = true;
            route.signal.controlled_emitter_offset_radius = 22.0f;
            route.signal.controlled_emitter_offset_jitter = 6.0f;
            // Food drops AT the emitter: a worker that follows the signal
            // gets a real meal. count > 0 + amount > 0 is the only
            // difference from SignalResponse on the world layer.
            route.signal.controlled_emitter_food_count = 2;
            route.signal.controlled_emitter_food_amount = 18.0f;
            route.signal.controlled_emitter_food_radius = 4.0f;
            route.objects.count = 6;
            route.tree.count = 1;
            route.campfire.count = 0;
            route.worksite.count = 0;
            route.survival.initial_hydration_ratio = std::max(route.survival.initial_hydration_ratio, 0.65f);
            break;

        case MemoryAblationTaskKind::CommDangerWarning:
            // A3.S3 - danger warning / safe-rallying task (Phase 2 redesign
            // 2026-05-22).
            //
            // Original Phase 1 design: emitter beacons near a predator pack
            // and listener must FLEE; signal_response_dense_reward = -0.6
            // tries to encode "follow signal -> bad". That design produced
            // survived=8/8 in all v8 ablation variants because (a)
            // randomPosition() spreads predators across the whole map so
            // they rarely contact protagonists in 200s, and (b) NEAT in
            // 120 generations did not learn to invert the listener-side
            // dense gradient.
            //
            // Phase 2 redesign: keep "without signal you die more often"
            // but flip the semantics to "signal = SAFE direction (rally
            // to nest)". Now the listener-side dense reward is positive
            // (same direction as comm_scout_worker), the task is trivially
            // learnable from v8's listener policy, and we get a clean
            // primary metric drop because:
            //   - emitter sits near the nest (small offset radius)
            //   - predators are dense and roam the whole map
            //   - protagonist with signal -> walks toward emitter -> hides
            //     near the nest -> survives
            //   - protagonist without signal -> wanders -> caught by a
            //     roaming predator -> dies
            //
            // Reward profile flip lives in configureMemoryAblationTaskParams
            // (signal_response_dense_reward 0.6, halo +1.5, dense +1.5).
            route.world_role = "eval";
            route.world_size = Vec2{60.0f, 60.0f};  // shrink map -> predators
                                                    // contact agents quickly
            route.base.center = Vec2{30.0f, 30.0f};
            route.base.radius = 10.0f;
            route.base.stockpile_radius = 6.0f;
            route.base.nest_radius = 4.0f;
            // Phase 3 (2026-05-22, B-stage L1): the magical
            // nest_predator_damage_per_second hack has been removed and
            // replaced by emergent shelter physics. Shelter now lives in
            // WorksiteLayer::enforcePredatorObstacles: completed worksites
            // act as collidable walls that predators cannot pass through.
            // For this task to read non-zero survived deltas, the
            // protagonist must learn to build worksites *before* the
            // predator wave arrives (proactive shelter building). v8 has
            // never been trained on that objective, so baseline survived
            // here is expected to regress to disabled levels until a v9
            // retrain teaches the proactive-build behaviour.
            route.background.forager_count = 0;
            route.background.predator_count = 12;  // 3 -> 12 (dense roaming)
            route.protagonist.predator_warn_enabled = false;
            route.resources.count = 4;
            route.water.source_count = 1;
            route.signal.protagonist_emit_enabled = false;
            route.signal.controlled_emitter_enabled = true;
            route.signal.controlled_emitter_period_seconds = 10.0;
            // Emitter near nest, deterministic position so the dense
            // gradient consistently points to safety.
            route.signal.controlled_emitter_randomize_position = false;
            route.signal.controlled_emitter_offset_radius = 6.0f;
            route.signal.controlled_emitter_offset_jitter = 0.0f;
            route.signal.controlled_emitter_food_count = 0;
            route.objects.count = 4;
            route.tree.count = 1;
            route.campfire.count = 0;
            route.worksite.count = 0;
            route.survival.initial_health_ratio = std::max(route.survival.initial_health_ratio, 0.90f);
            route.survival.initial_hydration_ratio = std::max(route.survival.initial_hydration_ratio, 0.70f);
            break;

        case MemoryAblationTaskKind::CommBuildRequest:
            // A3.S4 - build request task (Phase 2 redesign 2026-05-22).
            //
            // Original Phase 1 design demanded the listener also chop a
            // tree, carry a stick AND a stone, and walk to an invisible
            // worksite. v8 listeners followed the signal (responder=5.25/8,
            // fitness drop 63%) but no episode ever produced a
            // BuildCompleted because the chop->carry chain had not been
            // trained. The signal chain was provably alive but the primary
            // metric (worksite_completion_events) stayed pinned at 0.
            //
            // Phase 2 redesign 2026-05-22 (v2 iteration): keep the
            // signal-only worksite-direction-cue invariant but reuse the
            // chop-tree -> carry-stick -> deposit chain that v8 already
            // demonstrates can succeed on worksite_return. Earlier v1 try
            // with `tree.count=0` + nursery_supply auto-stick fell flat
            // because v8 never picks loose sticks up on its own; it only
            // gathers via ChopTreeCapability. We therefore restore tree
            // count and lower the build threshold so a single deposit
            // closes the worksite.
            //
            // Isolation strategy:
            //   - tree.count = 2 (same as worksite_return; v8 knows this
            //     chain)
            //   - worksite needs 1 stick, 0 stones
            //   - worksite.spawn_radius small so emitter + worksite are
            //     in a tight cluster near the nest
            //   - worksite.direction_requires_visibility stays true:
            //     the SIGNAL is still the only directional cue. Disable
            //     the signal and the listener cannot localise the worksite
            //     even though it knows how to chop + carry, so completion
            //     should drop.
            route.world_role = "eval";
            route.world_size = Vec2{120.0f, 120.0f};
            route.base.center = Vec2{44.0f, 60.0f};
            route.base.radius = 12.0f;
            route.base.stockpile_radius = 8.0f;
            route.base.nest_radius = 5.0f;
            // Keep at least one background agent so AgentSocialPerception stays
            // in the sensory dim (see CommScoutWorker comment above).
            route.background.forager_count = 1;
            route.background.predator_count = 0;
            route.protagonist.hunt_enabled = false;
            route.protagonist.goal_action_assist_enabled = true;
            route.resources.count = 6;
            route.water.source_count = 1;
            route.signal.protagonist_emit_enabled = false;
            route.signal.controlled_emitter_enabled = true;
            route.signal.controlled_emitter_period_seconds = 15.0;
            route.signal.controlled_emitter_randomize_position = false;
            route.signal.controlled_emitter_offset_radius = 0.0f;  // emit at worksite
            route.signal.controlled_emitter_offset_jitter = 0.0f;
            route.signal.controlled_emitter_food_count = 0;
            route.objects.count = 24;        // sticks + stones around nest
            route.tree.count = 2;            // restore chop chain
            route.campfire.count = 0;
            route.worksite.count = 1;
            route.worksite.spawn_radius = 8.0f;
            route.worksite.required_sticks = 1;       // 2 -> 1 (single deposit)
            route.worksite.required_stones = 0;       // 1 -> 0 (sticks only)
            route.worksite.assembly_rate = std::max(route.worksite.assembly_rate, 0.30f);
            // Worksite visibility off: signal is the only directional cue.
            route.worksite.direction_requires_visibility = true;
            // Phase 2 follow-up 2026-05-22: eval worlds keep all worksites
            // dormant by default (only live_role auto-activates the first
            // one), and progression is force-disabled on every memory
            // ablation task. Without preactivate the listener has no active
            // site to deposit into, which is what kept worksite_completion
            // pinned at 0 even after the goal-assist signal cue fix.
            route.worksite.preactivate = true;
            route.survival.initial_hydration_ratio = std::max(route.survival.initial_hydration_ratio, 0.80f);
            break;
    }

    return route;
}

}  // namespace neuro::routes::protagonist
