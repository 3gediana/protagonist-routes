#pragma once

#include "core/interfaces/IAgent.h"

#include "core/pathogen/Pathogen.h"
#include "core/world/layers/object/ObjectLayer.h"
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
    SimTimeSeconds ageSeconds() const override;
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
    SimTimeSeconds nextDecisionTimeSeconds() const;
    void setNextDecisionTimeSeconds(SimTimeSeconds time_seconds);
    bool shouldDecideAt(SimTimeSeconds current_time_seconds) const;

    void setSpeciesId(SpeciesId species_id);
    void addPerception(std::unique_ptr<IPerception> perception);
    void addCapability(std::unique_ptr<IAgentCapability> capability);
    void beginTick();
    void step(IWorld& world, float dt);
    void incrementAge(float dt);

    void noteFoodEaten(float amount);
    void notePlantFood(float amount);
    void noteMeatFood(float amount);
    void noteExplorationCell(std::size_t cell_index);
    void noteDamageDealt(float amount);
    void noteKill();
    float foodEatenThisTick() const;
    float plantFoodThisTick() const;
    float meatFoodThisTick() const;
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

    // Pathogen attached to this agent (null if not infected)
    Pathogen* pathogen() const;
    void attachPathogen(std::unique_ptr<Pathogen> p);

    // Immune memory: strain signatures of recovered infections
    // A new pathogen can infect if its strain is sufficiently different
    const std::vector<PathogenGenome::StrainSignature>& immuneMemory() const;
    void addImmuneMemory(const PathogenGenome::StrainSignature& strain);
    bool hasImmunityToStrain(const PathogenGenome::StrainSignature& strain, float threshold = 0.5f) const;

    bool isPredator() const;
    void setPredator(bool v);

    // Carried object (F-TOOL)
    ObjectId carriedObject() const { return carried_object_id_; }
    void setCarriedObject(ObjectId id) { carried_object_id_ = id; }
    bool isCarrying() const { return carried_object_id_ != 0; }

    bool hasModulatory() const { return has_modulatory_; }
    void setHasModulatory(bool v) { has_modulatory_ = v; }

    // Behavioral differentiation genes (evolvable)
    // habitat_pref: 0=lowland, 1=highland — reward for being in preferred terrain
    // activity_period: 0=nocturnal, 1=diurnal — energy regen varies with time of day
    // diet_pref: 0=herbivore, 1=carnivore — affects food/predation reward balance
    float habitatPreference() const { return habitat_pref_; }
    void setHabitatPreference(float v) { habitat_pref_ = v; }
    float activityPeriod() const { return activity_period_; }
    void setActivityPeriod(float v) { activity_period_ = v; }
    float dietPreference() const { return diet_pref_; }
    void setDietPreference(float v) { diet_pref_ = v; }

private:
    AgentId id_;
    GenomeId genome_id_;
    SpeciesId species_id_;
    std::unique_ptr<IBrain> brain_;
    Vec2 position_;
    Vec2 velocity_;
    float energy_;
    SimTimeSeconds age_seconds_;
    bool alive_;
    double fitness_;
    float decision_interval_;
    SimTimeSeconds next_decision_time_seconds_;
    std::vector<float> last_outputs_;
    float food_eaten_this_tick_;
    float plant_food_this_tick_;
    float meat_food_this_tick_;
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
    std::unique_ptr<Pathogen> pathogen_;
    std::vector<PathogenGenome::StrainSignature> immune_memory_;
    bool is_predator_;
    bool has_modulatory_ = false;
    ObjectId carried_object_id_ = 0;  // 0 = not carrying
    float habitat_pref_ = 0.5f;
    float activity_period_ = 0.5f;
    float diet_pref_ = 0.5f;
    std::vector<std::unique_ptr<IPerception>> perceptions_;
    std::vector<std::unique_ptr<IAgentCapability>> capabilities_;
};

}  // namespace neuro
