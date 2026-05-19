#pragma once

#include "core/interfaces/IAgent.h"

#include <memory>
#include <span>
#include <unordered_set>
#include <vector>

namespace neuro {

class IPerception;
class IAgentCapability;
class IWorld;

class Agent final : public IAgent {
public:
    Agent(AgentId id,
          GenomeId genome_id,
          std::unique_ptr<IBrain> brain,
          Vec2 position,
          float energy,
          std::size_t exploration_grid_size = 20,
          float decision_interval = 1.0f);
    ~Agent() override;

    AgentId id() const override;
    GenomeId genomeId() const override;
    SpeciesId speciesId() const override;
    Vec2 position() const override;
    Vec2 velocity() const override;
    void setPosition(Vec2 p) override;
    void setVelocity(Vec2 v) override;
    float energy() const override;
    void setEnergy(float e) override;
    Tick age() const override;
    bool alive() const override;
    void kill(const char* cause) override;
    IBrain& brain() override;
    const IBrain& brain() const override;
    double fitness() const override;
    void addFitness(double delta) override;
    void resetFitness() override;

    float decisionInterval() const;
    Tick nextDecisionTick() const;
    void setNextDecisionTick(Tick tick);
    bool shouldDecide(Tick current_tick) const;

    void setSpeciesId(SpeciesId species_id);
    void addPerception(std::unique_ptr<IPerception> perception);
    void addCapability(std::unique_ptr<IAgentCapability> capability);
    void beginTick();
    void step(IWorld& world, float dt);
    void incrementAge();

    void noteFoodEaten(float amount);
    void noteExplorationCell(std::size_t cell_index);
    void noteDamageDealt(float amount);
    void noteKill();
    float foodEatenThisTick() const;
    bool discoveredNewCellThisTick() const;
    std::size_t visitedCellCount() const;
    float damageDealt() const;
    std::size_t kills() const;

    bool infected() const;
    void setInfected(bool v);
    float infectionProgress() const;
    void advanceInfection(float dt);
    float immunityProgress() const;
    void advanceImmunity(float dt);
    bool immune() const;
    void setImmune(bool v);
    float immunityGainRate() const;
    void setImmunityGainRate(float rate);

    bool isPredator() const;
    void setPredator(bool v);

private:
    AgentId id_;
    GenomeId genome_id_;
    SpeciesId species_id_;
    std::unique_ptr<IBrain> brain_;
    Vec2 position_;
    Vec2 velocity_;
    float energy_;
    Tick age_;
    bool alive_;
    double fitness_;
    float decision_interval_;
    Tick next_decision_tick_;
    std::vector<float> last_outputs_;
    float food_eaten_this_tick_;
    bool discovered_new_cell_this_tick_;
    std::size_t exploration_grid_size_;
    std::unordered_set<std::size_t> visited_cells_;
    float damage_dealt_;
    std::size_t kills_;
    bool infected_;
    float infection_progress_;
    float immunity_progress_;
    bool immune_;
    float immunity_gain_rate_;
    bool is_predator_;
    std::vector<std::unique_ptr<IPerception>> perceptions_;
    std::vector<std::unique_ptr<IAgentCapability>> capabilities_;
};

}  // namespace neuro
