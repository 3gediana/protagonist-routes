#include "core/reward/NoveltyReward.h"
#include "core/agent/Agent.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"

namespace neuro {

NoveltyReward::NoveltyReward(double weight, std::shared_ptr<NoveltyArchive> archive)
    : weight_(weight), archive_(std::move(archive)) {}

double NoveltyReward::evaluate(const IAgent& agent, const IWorld& world, float dt) const {
    (void)world;
    (void)dt;

    BehaviorCharacterization bc;
    bc.final_x = agent.position().x;
    bc.final_y = agent.position().y;
    bc.total_distance = 0.0f;
    bc.avg_energy = agent.energy();
    bc.visited_cells_ratio = 0.0f;
    bc.food_eaten = 0.0f;
    bc.damage_dealt = 0.0f;
    bc.signals_emitted = 0.0f;

    const auto* concrete = dynamic_cast<const Agent*>(&agent);
    if (concrete) {
        bc.visited_cells_ratio = static_cast<float>(concrete->visitedCellCount());
        bc.food_eaten = concrete->foodEatenThisTick();
        bc.damage_dealt = concrete->damageDealt();
    }

    return archive_->novelty(bc) * weight_;
}

double NoveltyReward::weight() const { return weight_; }

const char* NoveltyReward::name() const { return "novelty"; }

void NoveltyReward::recordBehavior(const IAgent& agent) {
    BehaviorCharacterization bc;
    bc.final_x = agent.position().x;
    bc.final_y = agent.position().y;
    bc.total_distance = 0.0f;
    bc.avg_energy = agent.energy();
    bc.visited_cells_ratio = 0.0f;
    bc.food_eaten = 0.0f;
    bc.damage_dealt = 0.0f;
    bc.signals_emitted = 0.0f;

    const auto* concrete = dynamic_cast<const Agent*>(&agent);
    if (concrete) {
        bc.visited_cells_ratio = static_cast<float>(concrete->visitedCellCount());
        bc.food_eaten = concrete->foodEatenThisTick();
        bc.damage_dealt = concrete->damageDealt();
    }

    archive_->add(bc);
}

}  // namespace neuro
