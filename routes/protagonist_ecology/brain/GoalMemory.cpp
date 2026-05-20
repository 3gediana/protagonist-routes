#include "brain/GoalMemory.h"

#include <algorithm>

namespace neuro::routes::protagonist {

GoalMemory::GoalMemory() {
    reset();
}

void GoalMemory::reset() {
    state_.current_goal = GoalType::Explore;
    state_.commitment = 1.0f;
    state_.elapsed_seconds = 0.0f;
    state_.completion_progress = 0.0f;
}

void GoalMemory::tick(float dt) {
    state_.elapsed_seconds += dt;
    // Goal commitment naturally decays unless reinforced by positive outcomes
    state_.commitment = std::max(0.0f, state_.commitment - 0.015f * dt);
}

void GoalMemory::setGoal(GoalType goal, float commitment_reinforce) {
    if (state_.current_goal != goal) {
        state_.current_goal = goal;
        state_.commitment = std::clamp(commitment_reinforce, 0.0f, 1.0f);
        state_.elapsed_seconds = 0.0f;
        state_.completion_progress = 0.0f;
    } else {
        reinforceCommitment(commitment_reinforce);
    }
}

void GoalMemory::reinforceCommitment(float amount) {
    state_.commitment = std::clamp(state_.commitment + amount, 0.0f, 1.0f);
}

void GoalMemory::progress(float amount) {
    state_.completion_progress = std::clamp(state_.completion_progress + amount, 0.0f, 1.0f);
}

const char* goalTypeName(GoalType goal) {
    switch (goal) {
        case GoalType::Explore: return "explore";
        case GoalType::FindWater: return "find_water";
        case GoalType::FindFood: return "find_food";
        case GoalType::GatherStick: return "gather_stick";
        case GoalType::GatherStone: return "gather_stone";
        case GoalType::ChopTree: return "chop_tree";
        case GoalType::BuildWorksite: return "build_worksite";
        case GoalType::CraftWeapon: return "craft_weapon";
        case GoalType::Hunt: return "hunt";
        case GoalType::Flee: return "flee";
        case GoalType::RespondSignal: return "respond_signal";
        case GoalType::EmitSignal: return "emit_signal";
        case GoalType::RestNearFire: return "rest_near_fire";
        case GoalType::Count: return "count";
    }
    return "unknown";
}

}  // namespace neuro::routes::protagonist
