#include "core/reward/CombatReward.h"

#include "core/agent/Agent.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"

namespace neuro {

CombatReward::CombatReward(double weight, float attack_reward, float kill_bonus, float death_penalty)
    : weight_(weight),
      attack_reward_(attack_reward),
      kill_bonus_(kill_bonus),
      death_penalty_(death_penalty) {}

double CombatReward::evaluate(const IAgent& agent, const IWorld& world, float dt) const {
    (void)world;
    (void)dt;
    double reward = 0.0;

    const auto* concrete = dynamic_cast<const Agent*>(&agent);
    if (concrete == nullptr) return 0.0;

    reward += static_cast<double>(concrete->damageDealt()) * attack_reward_;
    reward += static_cast<double>(concrete->kills()) * kill_bonus_;

    if (!agent.alive()) {
        reward -= death_penalty_;
    }

    return reward;
}

double CombatReward::weight() const {
    return weight_;
}

const char* CombatReward::name() const {
    return "combat";
}

}  // namespace neuro
