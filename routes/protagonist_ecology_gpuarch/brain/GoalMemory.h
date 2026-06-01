#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace neuro::routes::protagonist {

enum class GoalType : int {
    Explore = 0,
    FindWater = 1,
    FindFood = 2,
    GatherStick = 3,
    GatherStone = 4,
    ChopTree = 5,
    BuildWorksite = 6,
    CraftWeapon = 7,
    Hunt = 8,
    Flee = 9,
    RespondSignal = 10,
    EmitSignal = 11,
    RestNearFire = 12,
    Count = 13
};

struct GoalMemoryState {
    GoalType current_goal{GoalType::Explore};
    float commitment{1.0f};             // 1.0 = fully committed, decays over time unless reinforced
    float elapsed_seconds{0.0f};        // How long we spent on this goal
    float completion_progress{0.0f};    // Evaluated progress
};

class GoalMemory {
public:
    GoalMemory();

    void reset();
    void tick(float dt);

    void setGoal(GoalType goal, float commitment_reinforce = 1.0f);
    void reinforceCommitment(float amount);
    void progress(float amount);

    GoalType currentGoal() const { return state_.current_goal; }
    float commitment() const { return state_.commitment; }
    float elapsedSeconds() const { return state_.elapsed_seconds; }
    float elapsedTicks() const { return state_.elapsed_seconds; }
    float completionProgress() const { return state_.completion_progress; }

    const GoalMemoryState& state() const { return state_; }

private:
    GoalMemoryState state_;
};

const char* goalTypeName(GoalType goal);

}  // namespace neuro::routes::protagonist
