#include "capability/ChopTreeCapability.h"

#include "core/agent/Agent.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "world/TreeLayer.h"

namespace neuro::routes::protagonist {

ChopTreeCapability::ChopTreeCapability(float chop_radius, float damage, float energy_cost, float activation_threshold,
                                       float progress_reward)
    : chop_radius_(chop_radius),
      damage_(damage),
      energy_cost_(energy_cost),
      activation_threshold_(activation_threshold),
      progress_reward_(progress_reward) {}

void ChopTreeCapability::closeSession(IAgent& agent, IWorld& world, float remaining_integrity) {
    if (session_tree_id_ == 0) {
        return;
    }
    const double now = world.currentTimeSeconds();
    world.eventBus().publish(events::Event{
        events::EventType::ChopAbandoned,
        events::ChopAbandoned{
            agent.id(),
            session_tree_id_,
            session_started_seconds_,
            now - session_started_seconds_,
            remaining_integrity},
        world.currentTick(),
        world.currentTimeSeconds()});
    session_tree_id_ = 0;
}

void ChopTreeCapability::apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) {
    // Output below threshold: agent is not asking to chop. Close any open
    // session as Abandoned (we lost the trailing integrity from somewhere
    // mid-chop) and exit early.
    if (outputs.empty() || outputs[0] < activation_threshold_) {
        if (session_tree_id_ != 0) {
            closeSession(agent, world, /*remaining_integrity=*/0.0f);
        }
        return;
    }
    chop_attempts.fetch_add(1, std::memory_order_relaxed);

    auto* concrete = dynamic_cast<Agent*>(&agent);
    auto* trees = world.getLayer<TreeLayer>();
    if (concrete == nullptr || trees == nullptr) {
        if (session_tree_id_ != 0) {
            closeSession(agent, world, /*remaining_integrity=*/0.0f);
        }
        return;
    }

    // Carrying no longer hard-blocks chopping; one hand is busy so swings
    // land at half strength. This keeps the chop->carry->build chain fluid
    // instead of forcing a drop/pickup detour for every extra log.
    const float carry_mult = concrete->isCarrying() ? 0.5f : 1.0f;
    const ChopHitResult hit = trees->chopNearest(agent.position(), chop_radius_, damage_ * carry_mult * dt, world);
    if (hit.tree_id == 0) {
        // No tree in range this tick. If we had a session, the agent walked
        // away mid-chop -> abandon.
        if (session_tree_id_ != 0) {
            closeSession(agent, world, /*remaining_integrity=*/0.0f);
        }
        return;
    }

    const double now = world.currentTimeSeconds();

    // Session bookkeeping: handle start / continuation / target switch.
    if (session_tree_id_ == 0) {
        session_tree_id_ = hit.tree_id;
        session_started_seconds_ = now;
        session_last_active_seconds_ = now;
        chop_started_events.fetch_add(1, std::memory_order_relaxed);
        world.eventBus().publish(events::Event{
            events::EventType::ChopStarted,
            events::ChopStarted{agent.id(), hit.tree_id, hit.tree_pos, now},
            world.currentTick(),
            world.currentTimeSeconds()});
    } else if (session_tree_id_ != hit.tree_id) {
        // Target switched mid-chop. Close old session as abandoned, start a
        // new one on the new tree.
        closeSession(agent, world, hit.remaining_integrity);
        session_tree_id_ = hit.tree_id;
        session_started_seconds_ = now;
        session_last_active_seconds_ = now;
        chop_started_events.fetch_add(1, std::memory_order_relaxed);
        world.eventBus().publish(events::Event{
            events::EventType::ChopStarted,
            events::ChopStarted{agent.id(), hit.tree_id, hit.tree_pos, now},
            world.currentTick(),
            world.currentTimeSeconds()});
    } else {
        session_last_active_seconds_ = now;
    }

    concrete->setEnergy(concrete->energy() - energy_cost_ * dt);

    // D-136 audit (chop progress PBRS): pay dense fitness per point of
    // integrity actually removed, so every swing that lands on a real tree
    // moves fitness, not just the final felling tick. Felling a tree takes
    // tens of consecutive correct ticks; with reward only on `felled`, NEAT
    // had no selection gradient on partial chops (0.18% success plateau).
    if (progress_reward_ > 0.0f) {
        concrete->addFitness(static_cast<double>(progress_reward_)
                             * static_cast<double>(damage_ * carry_mult * dt));
    }

    if (hit.felled) {
        chop_successes.fetch_add(1, std::memory_order_relaxed);
        chop_completed_events.fetch_add(1, std::memory_order_relaxed);
        // L2.7 v13 HRL sub-reward: ChopTree (GoalType=5) completed.
        concrete->noteGoalCompletion(5);
        world.eventBus().publish(events::Event{
            events::EventType::ChopCompleted,
            events::ChopCompleted{
                agent.id(),
                hit.tree_id,
                hit.tree_pos,
                session_started_seconds_,
                now - session_started_seconds_,
                hit.wood_yield},
            world.currentTick(),
            world.currentTimeSeconds()});
        session_tree_id_ = 0;
    }
}

}  // namespace neuro::routes::protagonist
