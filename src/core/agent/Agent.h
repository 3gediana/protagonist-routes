#pragma once

#include "core/interfaces/IAgent.h"

#include "core/pathogen/Pathogen.h"
#include "core/world/layers/ResourceLayer.h"
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
    void decide(IWorld& world, float dt);
    // Phase 1 of batched decision: run perception sense, then if brain is
    // batchable call prepareInputs() (NO dense matmul yet); else stash inputs
    // for phase 3 to run brain.forward() inline.
    // Returns true iff this agent's brain decided this tick (so the runner
    // knows to include it in the batch).
    bool decideUntilDense(IWorld& world, float dt);
    // Phase 3 of batched decision: if brain is batchable call finalizeOutputs();
    // else run brain.forward() on stashed inputs. Either way writes last_outputs_.
    void decideAfterDense(IWorld& world, float dt);
    bool batchedDecisionPending() const;
    void publishDecisionEvent(IWorld& world);
    void applyCapabilities(IWorld& world, float dt);
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

    // ---- L2.1 species differentiation v2 (SPECIES_DIFFERENTIATION_SPEC.md) ----
    // Prey-side damage threshold. When > 0, attackers must accumulate
    // damage in a sliding window >= threshold to land a "kill burst".
    // Single attacker with damage_per_window < threshold cannot kill
    // alone, forcing emergent group attack (wolfpack). When 0 (default),
    // direct damage is applied (legacy path).
    float damageThreshold() const { return damage_threshold_; }
    void setDamageThreshold(float v) { damage_threshold_ = v; }

    // Threat-damage window: per-attacker hits with timestamps. Used as
    // the prey-side accumulator for damageThreshold(). Constant time
    // O(W) where W is window size in ticks (typically <= 30 entries).
    struct ThreatHit {
        AgentId attacker;
        float damage;
        SimTimeSeconds time;
    };
    void accumulateThreatDamage(AgentId attacker, float damage, SimTimeSeconds time);
    float threatDamageInWindow(SimTimeSeconds current_time, SimTimeSeconds window) const;
    std::size_t threatDamageContributorCount(SimTimeSeconds current_time, SimTimeSeconds window) const;
    void clearThreatDamageWindow();
    const std::vector<ThreatHit>& threatDamageHistory() const { return threat_damage_window_; }

    // Stealth state (leopard ambush). When true, AgentSocialPerception
    // filters self out for observers that are not directly adjacent
    // (< 2m). Set/cleared by StealthCapability based on stationary time.
    bool isStealth() const { return stealth_state_; }
    void setStealth(bool v) { stealth_state_ = v; }

    // Stationary time accumulator (used by StealthCapability for the
    // 3s static->stealth threshold). Reset whenever the agent moves
    // faster than ~0.05 units/sec.
    SimTimeSeconds stationarySeconds() const { return stationary_seconds_; }
    void setStationarySeconds(SimTimeSeconds v) { stationary_seconds_ = v; }

    // L2.6 v12 (context-conditioned policy, PEARL/RL² style): which
    // task_template the protagonist is currently in. Filled by the
    // Runtime after applyTaskTemplate() picks a template. Read by
    // TaskContextPerception which exposes it to the brain as an
    // 8-onehot input, so the network can learn task-specific behaviour
    // (proactive_build vs solo_hunt vs mixed_ecology) without needing
    // to delete templates from the pool. -1 = no template active.
    int taskTemplateIndex() const { return task_template_idx_; }
    void setTaskTemplateIndex(int idx) { task_template_idx_ = idx; }

    // Digest satiety (apex predator). After a big kill, set a "satiety
    // until" timestamp. While current_time < this value, hunt damage
    // and energy gain are scaled down (apex sleeps off the kill, the
    // base-state form of L1's fatigue drive).
    SimTimeSeconds digestSatietyUntilSeconds() const { return digest_satiety_until_seconds_; }
    void setDigestSatietyUntilSeconds(SimTimeSeconds t) { digest_satiety_until_seconds_ = t; }
    bool inDigestSatiety(SimTimeSeconds now) const { return digest_satiety_until_seconds_ > now; }

    // Scavenge efficiency. Multiplier applied to energy gain when this
    // agent attacks low-vitality prey (energy below 20% of damage
    // threshold). Default 1.0. Scavenger species set this to 3.0.
    float scavengeEfficiency() const { return scavenge_efficiency_; }
    void setScavengeEfficiency(float v) { scavenge_efficiency_ = v; }

    // In-cave hide flag (small forager). Updated each tick by a small
    // capability that queries TerrainLayer. AgentSocialPerception filters
    // hidden prey out for observers that are themselves outside caves.
    bool isInCave() const { return is_in_cave_; }
    void setInCave(bool v) { is_in_cave_ = v; }

    // ---- L2.2 homeostatic drive state v2 (SPECIES_DIFFERENTIATION_SPEC.md § 2) ----
    // Five drives per Hull / Barto IMRL / City Univ eLife 2014 / HAL HRL 2021.
    // Reward formula (per spec § 2.2):
    //   reward_t = - sum_drives ( drive_i_t - setpoint_i )^2 + outcome_bonuses
    // Each drive is in [0,1]; setpoint is the species-specific "comfortable"
    // value (e.g. ApexPredator loneliness_setpoint=0.6 means apex prefers
    // to be alone, peers nearby drop loneliness_drive below 0.6 and reward
    // turns negative).
    //
    // hunger    : grows per second of metabolism, drops on food eaten
    // thirst    : grows per second, drops on drink (mirrors `hydration` in
    //             survival layer; we keep it as a separate normalised drive
    //             value here so all drives share the same units)
    // fatigue   : grows with movement / hunt activity; drops on rest
    // fear      : grows when predator in sense radius; decays without
    // loneliness: grows when no peer in sense radius; drops with peer near
    struct DriveState {
        float hunger     = 0.0f;
        float thirst     = 0.0f;
        float fatigue    = 0.0f;
        float fear       = 0.0f;
        float loneliness = 0.0f;
    };
    struct DriveSetpoints {
        float hunger     = 0.0f;
        float thirst     = 0.0f;
        float fatigue    = 0.0f;
        float fear       = 0.0f;
        float loneliness = 0.0f;
    };

    const DriveState& driveState() const { return drives_; }
    DriveState& driveState() { return drives_; }
    void setDriveState(const DriveState& s) { drives_ = s; }

    const DriveSetpoints& driveSetpoints() const { return drive_setpoints_; }
    void setDriveSetpoints(const DriveSetpoints& s) { drive_setpoints_ = s; }

    // Drive-delta reward at the current state (used by per-tick reward
    // computation): -((d-s)^2) summed across the 5 drives. 0 when all
    // drives are at their setpoints. Always <= 0.
    float driveSetpointPenalty() const {
        const float dh = drives_.hunger     - drive_setpoints_.hunger;
        const float dt = drives_.thirst     - drive_setpoints_.thirst;
        const float df = drives_.fatigue    - drive_setpoints_.fatigue;
        const float dr = drives_.fear       - drive_setpoints_.fear;
        const float dl = drives_.loneliness - drive_setpoints_.loneliness;
        return -(dh*dh + dt*dt + df*df + dr*dr + dl*dl);
    }

    // ---- L2.3 resource niche partitioning v2 (SPECIES_DIFFERENTIATION_SPEC.md § 3) ----
    // Per-kind energy-gain multiplier when this agent eats. Default 1.0
    // = neutral (no preference). EatCapability multiplies its gain by
    // eatEfficiency(consume.kind), so e.g. LargeHerbivore with eat_eff_browse
    // = 0.5 only gets half the energy from a Browse cell vs a Grass
    // cell, capturing kudu / nyala / bushbuck-style niche separation
    // (HHMI / Chesson 2000).
    struct EatEfficiencyByKind {
        float grass  = 1.0f;
        float browse = 1.0f;
        float fruit  = 1.0f;
        float meat   = 1.0f;
    };
    const EatEfficiencyByKind& eatEfficiency() const { return eat_efficiency_; }
    void setEatEfficiency(const EatEfficiencyByKind& e) { eat_efficiency_ = e; }
    float eatEfficiencyOf(ResourceKind kind) const {
        switch (kind) {
            case ResourceKind::Grass:  return eat_efficiency_.grass;
            case ResourceKind::Browse: return eat_efficiency_.browse;
            case ResourceKind::Fruit:  return eat_efficiency_.fruit;
            case ResourceKind::Meat:   return eat_efficiency_.meat;
        }
        return 1.0f;
    }

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
    bool decision_triggered_this_tick_;
    // Batched-decision stash: between decideUntilDense() and decideAfterDense().
    std::vector<float> pending_inputs_;
    float pending_dt_ = 0.0f;
    bool pending_decision_ = false;
    bool pending_uses_batch_ = false;
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
    // L2.1 species differentiation
    std::vector<ThreatHit> threat_damage_window_;
    float damage_threshold_ = 0.0f;
    bool stealth_state_ = false;
    int task_template_idx_ = -1;
    SimTimeSeconds stationary_seconds_ = 0.0;
    SimTimeSeconds digest_satiety_until_seconds_ = -1.0;
    float scavenge_efficiency_ = 1.0f;
    bool is_in_cave_ = false;
    // L2.2 homeostatic drive state
    DriveState drives_{};
    DriveSetpoints drive_setpoints_{};
    // L2.3 resource niche
    EatEfficiencyByKind eat_efficiency_{};
    std::vector<std::unique_ptr<IPerception>> perceptions_;
    std::vector<std::unique_ptr<IAgentCapability>> capabilities_;
};

}  // namespace neuro
