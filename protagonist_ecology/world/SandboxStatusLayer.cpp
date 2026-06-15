#include "world/SandboxStatusLayer.h"

#include "core/agent/Agent.h"
#include "capability/ChopTreeCapability.h"
#include "capability/CraftSpearCapability.h"
#include "capability/ProtagonistHuntCapability.h"
#include "capability/ThrowCarriedObjectCapability.h"
#include "core/interfaces/IWorld.h"
#include "core/logging/Logger.h"
#include "core/world/layers/AgentLayer.h"
#include "core/world/layers/object/ObjectLayer.h"
#include "world/BaseLayer.h"
#include "world/CampfireLayer.h"
#include "world/InterventionDirectorLayer.h"
#include "world/NurseryLogisticsLayer.h"
#include "world/TreeLayer.h"
#include "world/WorksiteLayer.h"
#include "world/SurvivalStatusLayer.h"

namespace neuro::routes::protagonist {

SandboxStatusLayer::SandboxStatusLayer(std::size_t log_interval_ticks)
    : log_interval_ticks_(log_interval_ticks == 0 ? 1u : log_interval_ticks) {}

void SandboxStatusLayer::onAttach(IWorld& world) {
    agents_ = world.getLayer<AgentLayer>();
    base_ = world.getLayer<BaseLayer>();
    campfire_ = world.getLayer<CampfireLayer>();
    director_ = world.getLayer<InterventionDirectorLayer>();
    logistics_ = world.getLayer<NurseryLogisticsLayer>();
    objects_ = world.getLayer<ObjectLayer>();
    survival_ = world.getLayer<SurvivalStatusLayer>();
    tree_ = world.getLayer<TreeLayer>();
    worksite_ = world.getLayer<WorksiteLayer>();
}

void SandboxStatusLayer::tick(IWorld& world, float dt) {
    (void)dt;
    if (world.currentTick() == 1 || (log_interval_ticks_ > 0 && world.currentTick() % log_interval_ticks_ == 0)) {
        logStatus(world);
    }
}

void SandboxStatusLayer::logStatus(const IWorld& world) const {
    std::size_t carrying_agents = 0;
    std::size_t carrying_stone_agents = 0;
    std::size_t carrying_spear_agents = 0;
    std::size_t carrying_heavy_agents = 0;
    if (agents_ != nullptr) {
        for (const auto* agent : agents_->agents()) {
            if (agent != nullptr && agent->alive() && agent->isCarrying()) {
                ++carrying_agents;
                if (objects_ != nullptr) {
                    if (const auto* carried = objects_->findById(agent->carriedObject()); carried != nullptr && carried->type == ObjectType::Stone) {
                        ++carrying_stone_agents;
                        if (carried->heavy) {
                            ++carrying_heavy_agents;
                        }
                    } else if (const auto* carried = objects_->findById(agent->carriedObject()); carried != nullptr && carried->type == ObjectType::Spear) {
                        ++carrying_spear_agents;
                    }
                }
            }
        }
    }

    std::size_t loose_sticks_near_nest = 0;
    std::size_t loose_sticks_on_path = 0;
    std::size_t loose_stones_near_nest = 0;
    std::size_t loose_stones_total = 0;
    std::size_t loose_heavy_stones_total = 0;
    std::size_t heavy_ready_max = 0;
    std::size_t active_worksite_sticks = 0;
    std::size_t active_worksite_stones = 0;
    std::size_t stockpiled_spears = logistics_ != nullptr ? logistics_->stockpiledCount(ObjectType::Spear) : 0u;
    std::size_t dehydrated_agents = survival_ != nullptr ? survival_->dehydratedCount() : 0u;
    std::size_t injured_agents = survival_ != nullptr ? survival_->injuredCount() : 0u;
    float avg_hydration = survival_ != nullptr ? survival_->averageHydrationRatio() : 1.0f;
    float avg_health = survival_ != nullptr ? survival_->averageHealthRatio() : 1.0f;
    float active_worksite_progress = 0.0f;
    std::size_t available_trees = tree_ != nullptr ? tree_->availableTreeCount() : 0u;
    std::size_t harvested_trees = tree_ != nullptr ? tree_->harvestedCount() : 0u;
    if (objects_ != nullptr && base_ != nullptr) {
        const float near_nest_sq = 16.0f * 16.0f;
        const Vec2 trail_start = base_->nestCenter();
        const Vec2 trail_end = base_->stockpileCenter();
        const Vec2 trail = trail_end - trail_start;
        const float trail_len_sq = trail.x * trail.x + trail.y * trail.y;
        for (const auto& object : objects_->objects()) {
            if (object.carried || base_->inStockpile(object.pos)) {
                continue;
            }
            if (object.type == ObjectType::Stone) {
                ++loose_stones_total;
                if (object.heavy) {
                    ++loose_heavy_stones_total;
                    if (agents_ != nullptr) {
                        std::size_t nearby_helpers = 0;
                        for (const auto* agent : agents_->agents()) {
                            if (agent == nullptr || !agent->alive() || agent->isCarrying()) {
                                continue;
                            }
                            if (Vec2::distanceSquared(agent->position(), object.pos) <= 7.5f * 7.5f) {
                                ++nearby_helpers;
                            }
                        }
                        heavy_ready_max = std::max(heavy_ready_max, nearby_helpers);
                    }
                }
                if (Vec2::distanceSquared(object.pos, base_->nestCenter()) <= near_nest_sq) {
                    ++loose_stones_near_nest;
                }
            }
            if (object.type != ObjectType::Stick) {
                continue;
            }
            if (Vec2::distanceSquared(object.pos, base_->nestCenter()) <= near_nest_sq) {
                ++loose_sticks_near_nest;
            }
            if (trail_len_sq > 0.0001f) {
                const Vec2 relative = object.pos - trail_start;
                const float t = std::clamp((relative.x * trail.x + relative.y * trail.y) / trail_len_sq, 0.0f, 1.0f);
                const Vec2 projection = trail_start + trail * t;
                if (Vec2::distanceSquared(object.pos, projection) <= 4.0f * 4.0f) {
                    ++loose_sticks_on_path;
                }
            }
        }
    }
    if (worksite_ != nullptr) {
        for (const auto& site : worksite_->sites()) {
            if (!site.active || site.completed) {
                continue;
            }
            active_worksite_sticks = site.stored_sticks;
            active_worksite_stones = site.stored_stones;
            active_worksite_progress = site.build_progress;
            break;
        }
    }

    LOG_INFO(
        "SandboxStatus: tick={} deposits_total={} deposits_stick={} deposits_stone={} stock_stick={} stock_stone={} stock_spear={} carrying_agents={} loose_sticks_near_nest={} loose_sticks_on_path={} active_fires={} active_worksites={} completed_worksites={} available_trees={} harvested_trees={} dehydrated={} injured={} avg_hydration={:.2f} avg_health={:.2f} intervention_budget={:.3f} intervention_stress={:.3f} intervention_pending={}",
        world.currentTick(),
        logistics_ != nullptr ? logistics_->totalDeposits() : 0u,
        logistics_ != nullptr ? logistics_->totalDeposits(ObjectType::Stick) : 0u,
        logistics_ != nullptr ? logistics_->totalDeposits(ObjectType::Stone) : 0u,
        logistics_ != nullptr ? logistics_->stockpiledCount(ObjectType::Stick) : 0u,
        logistics_ != nullptr ? logistics_->stockpiledCount(ObjectType::Stone) : 0u,
        stockpiled_spears,
        carrying_agents,
        loose_sticks_near_nest,
        loose_sticks_on_path,
        campfire_ != nullptr ? campfire_->activeFireCount() : 0u,
        worksite_ != nullptr ? worksite_->activeSiteCount() : 0u,
        worksite_ != nullptr ? worksite_->completedSiteCount() : 0u,
        available_trees,
        harvested_trees,
        dehydrated_agents,
        injured_agents,
        avg_hydration,
        avg_health,
        director_ != nullptr ? director_->budget() : 0.0f,
        director_ != nullptr ? director_->stress() : 0.0f,
        director_ != nullptr ? director_->pendingCount() : 0u);
    LOG_INFO(
        "SandboxDiag: tick={} deposits={} carry={} carry_stone={} carry_spear={} carry_heavy={} near={} stones_near={} path={} stones={} heavy_loose={} heavy_ready_max={} trees={} harvested={} spears={} dehydrated={} injured={} hydration={:.2f} health={:.2f} hunt_attempts={} hunt_successes={} chop_attempts={} chop_successes={} craft_attempts={} craft_successes={} throw_attempts={} throw_hits={} site_sticks={} site_stones={} site_progress={:.2f} fires={}",
        world.currentTick(),
        logistics_ != nullptr ? logistics_->totalDeposits() : 0u,
        carrying_agents,
        carrying_stone_agents,
        carrying_spear_agents,
        carrying_heavy_agents,
        loose_sticks_near_nest,
        loose_stones_near_nest,
        loose_sticks_on_path,
        loose_stones_total,
        loose_heavy_stones_total,
        heavy_ready_max,
        available_trees,
        harvested_trees,
        stockpiled_spears,
        dehydrated_agents,
        injured_agents,
        avg_hydration,
        avg_health,
        ProtagonistHuntCapability::hunt_attempts.load(),
        ProtagonistHuntCapability::hunt_successes.load(),
        ChopTreeCapability::chop_attempts.load(),
        ChopTreeCapability::chop_successes.load(),
        CraftSpearCapability::craft_attempts.load(),
        CraftSpearCapability::craft_successes.load(),
        ThrowCarriedObjectCapability::throw_attempts.load(),
        ThrowCarriedObjectCapability::throw_hits.load(),
        active_worksite_sticks,
        active_worksite_stones,
        active_worksite_progress,
        campfire_ != nullptr ? campfire_->activeFireCount() : 0u);
}

}  // namespace neuro::routes::protagonist
