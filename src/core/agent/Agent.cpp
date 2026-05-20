#include "core/agent/Agent.h"

#include "core/events/Event.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IBrain.h"
#include "core/interfaces/IAgentCapability.h"
#include "core/interfaces/IPerception.h"
#include "core/interfaces/IWorld.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace neuro {

Agent::Agent(AgentId id,
             GenomeId genome_id,
             std::unique_ptr<IBrain> brain,
             Vec2 position,
             float energy,
             std::size_t exploration_grid_size,
             float decision_interval)
    : id_(id),
      genome_id_(genome_id),
      species_id_(kInvalidSpeciesId),
      brain_(std::move(brain)),
      position_(position),
      velocity_(0.0f, 0.0f),
      energy_(energy),
      age_seconds_(0.0),
      alive_(true),
      fitness_(0.0),
      decision_interval_(std::max(0.0f, decision_interval)),
      next_decision_time_seconds_(0.0),
      food_eaten_this_tick_(0.0f),
      plant_food_this_tick_(0.0f),
      meat_food_this_tick_(0.0f),
      discovered_new_cell_this_tick_(false),
      exploration_grid_size_(exploration_grid_size),
      damage_dealt_(0.0f),
      kills_(0),
      infected_(false),
      infection_progress_(0.0f),
      immunity_progress_(0.0f),
      immune_(false),
      immunity_gain_rate_(0.05f),
      is_predator_(false),
      perceptions_(),
      capabilities_() {
}

Agent::~Agent() = default;

AgentId Agent::id() const { return id_; }
GenomeId Agent::genomeId() const { return genome_id_; }
SpeciesId Agent::speciesId() const { return species_id_; }
Vec2 Agent::position() const { return position_; }
Vec2 Agent::velocity() const { return velocity_; }
void Agent::setPosition(Vec2 p) { position_ = p; }
void Agent::setVelocity(Vec2 v) { velocity_ = v; }
float Agent::energy() const { return energy_; }
void Agent::setEnergy(float e) { energy_ = e; }
Tick Agent::age() const { return age_seconds_ <= 0.0 ? Tick{0} : static_cast<Tick>(std::floor(age_seconds_)); }
SimTimeSeconds Agent::ageSeconds() const { return age_seconds_; }
bool Agent::alive() const { return alive_; }
IBrain& Agent::brain() { return *brain_; }
const IBrain& Agent::brain() const { return *brain_; }
double Agent::fitness() const { return fitness_; }
void Agent::addFitness(double delta) { fitness_ += delta; }
void Agent::resetFitness() { fitness_ = 0.0; }

void Agent::kill(const char*) {
    alive_ = false;
}

float Agent::decisionInterval() const { return decision_interval_; }
Tick Agent::nextDecisionTick() const {
    return next_decision_time_seconds_ <= 0.0 ? Tick{0} : static_cast<Tick>(std::floor(next_decision_time_seconds_));
}
void Agent::setNextDecisionTick(Tick tick) { next_decision_time_seconds_ = static_cast<SimTimeSeconds>(tick); }
bool Agent::shouldDecide(Tick current_tick) const {
    return shouldDecideAt(static_cast<SimTimeSeconds>(current_tick));
}
SimTimeSeconds Agent::nextDecisionTimeSeconds() const { return next_decision_time_seconds_; }
void Agent::setNextDecisionTimeSeconds(SimTimeSeconds time_seconds) { next_decision_time_seconds_ = std::max<SimTimeSeconds>(0.0, time_seconds); }
bool Agent::shouldDecideAt(SimTimeSeconds current_time_seconds) const {
    return current_time_seconds + 1.0e-9 >= next_decision_time_seconds_;
}

void Agent::setSpeciesId(SpeciesId species_id) {
    species_id_ = species_id;
}

void Agent::addPerception(std::unique_ptr<IPerception> perception) {
    perceptions_.push_back(std::move(perception));
}

void Agent::addCapability(std::unique_ptr<IAgentCapability> capability) {
    capabilities_.push_back(std::move(capability));
}

void Agent::beginTick() {
    food_eaten_this_tick_ = 0.0f;
    plant_food_this_tick_ = 0.0f;
    meat_food_this_tick_ = 0.0f;
    discovered_new_cell_this_tick_ = false;
}

void Agent::step(IWorld& world, float dt) {
    if (!alive_) return;

    if (shouldDecideAt(world.currentTimeSeconds())) {
        std::size_t input_dim = 0;
        for (const auto& perception : perceptions_) {
            input_dim += perception->dim();
        }

        std::vector<float> inputs(input_dim, 0.0f);
        std::size_t offset = 0;
        for (const auto& perception : perceptions_) {
            auto view = std::span<float>(inputs.data() + offset, perception->dim());
            perception->sense(*this, world, view);
            offset += perception->dim();
        }

        if (brain_->inputDim() != inputs.size()) {
            throw std::runtime_error("Agent perception input dim mismatch");
        }
        auto output_span = brain_->forward(inputs, dt);
        last_outputs_.assign(output_span.begin(), output_span.end());
        next_decision_time_seconds_ = world.currentTimeSeconds() + static_cast<SimTimeSeconds>(decision_interval_);
        world.eventBus().publish(events::Event{events::EventType::DecisionTriggered,
                                               events::DecisionTriggered{ id_, world.currentTick() },
                                               world.currentTick(),
                                               world.currentTimeSeconds()});
    }

    if (!last_outputs_.empty()) {
        std::size_t output_offset = 0;
        for (auto& capability : capabilities_) {
            const auto consumed = capability->consumedOutputs();
            if (output_offset + consumed > last_outputs_.size()) {
                throw std::runtime_error("Agent capability output slice overflow");
            }
            std::span<const float> slice(last_outputs_.data() + output_offset, consumed);
            capability->apply(*this, world, slice, dt);
            output_offset += consumed;
        }

        if (output_offset != last_outputs_.size()) {
            throw std::runtime_error("Agent capability outputs do not fully consume brain outputs");
        }
    }
}

void Agent::incrementAge(float dt) {
    age_seconds_ += static_cast<SimTimeSeconds>(std::max(0.0f, dt));
}

void Agent::noteFoodEaten(float amount) {
    food_eaten_this_tick_ += std::max(0.0f, amount);
}

void Agent::notePlantFood(float amount) {
    noteFoodEaten(amount);
    plant_food_this_tick_ += std::max(0.0f, amount);
}

void Agent::noteMeatFood(float amount) {
    noteFoodEaten(amount);
    meat_food_this_tick_ += std::max(0.0f, amount);
}

void Agent::noteExplorationCell(std::size_t cell_index) {
    auto [_, inserted] = visited_cells_.insert(cell_index);
    discovered_new_cell_this_tick_ = discovered_new_cell_this_tick_ || inserted;
}

float Agent::foodEatenThisTick() const {
    return food_eaten_this_tick_;
}

float Agent::plantFoodThisTick() const {
    return plant_food_this_tick_;
}

float Agent::meatFoodThisTick() const {
    return meat_food_this_tick_;
}

bool Agent::discoveredNewCellThisTick() const {
    return discovered_new_cell_this_tick_;
}

std::size_t Agent::visitedCellCount() const {
    return visited_cells_.size();
}

void Agent::noteDamageDealt(float amount) {
    damage_dealt_ += amount;
}

void Agent::noteKill() {
    kills_++;
}

float Agent::damageDealt() const {
    return damage_dealt_;
}

std::size_t Agent::kills() const {
    return kills_;
}

bool Agent::infected() const {
    return infected_;
}

void Agent::setInfected(bool v) {
    infected_ = v;
    if (v) {
        immune_ = false;
        infection_progress_ = 0.0f;
        immunity_progress_ = 0.0f;
        return;
    }
    infection_progress_ = 0.0f;
    immunity_progress_ = 0.0f;
    pathogen_.reset();
    immune_ = !immune_memory_.empty();
}

float Agent::infectionProgress() const {
    return infection_progress_;
}

void Agent::advanceInfection(float dt) {
    infection_progress_ += dt;
}

float Agent::immunityProgress() const {
    return immunity_progress_;
}

void Agent::advanceImmunity(float dt) {
    immunity_progress_ += dt;
    if (immunity_progress_ > 1.0f) immunity_progress_ = 1.0f;
}

bool Agent::immune() const {
    return immune_ || (!infected_ && !immune_memory_.empty());
}

void Agent::setImmune(bool v) {
    immune_ = v;
}

float Agent::immunityGainRate() const {
    return immunity_gain_rate_;
}

void Agent::setImmunityGainRate(float rate) {
    immunity_gain_rate_ = std::max(rate, 0.0f);
}

Pathogen* Agent::pathogen() const {
    return pathogen_.get();
}

void Agent::attachPathogen(std::unique_ptr<Pathogen> p) {
    pathogen_ = std::move(p);
    if (pathogen_) {
        infected_ = true;
        immune_ = false;
        infection_progress_ = 0.0f;
        immunity_progress_ = 0.0f;
    }
}

const std::vector<PathogenGenome::StrainSignature>& Agent::immuneMemory() const {
    return immune_memory_;
}

void Agent::addImmuneMemory(const PathogenGenome::StrainSignature& strain) {
    for (const auto& existing : immune_memory_) {
        if (existing == strain) {
            immune_ = true;
            return;
        }
    }
    immune_memory_.push_back(strain);
    immune_ = true;
}

bool Agent::hasImmunityToStrain(const PathogenGenome::StrainSignature& strain, float threshold) const {
    // Check Euclidean distance between strain and all immune memories
    // If any memory is close enough, host has immunity
    for (const auto& mem : immune_memory_) {
        float dist_sq = 0.0f;
        for (std::size_t i = 0; i < PathogenGenome::kStrainDim; ++i) {
            const float d = strain[i] - mem[i];
            dist_sq += d * d;
        }
        // Max possible distance in 4D unit hypercube is 2.0 (sqrt(4))
        // threshold=0.5 means distance < 0.5 → immune
        if (dist_sq < threshold * threshold) return true;
    }
    return false;
}

bool Agent::isPredator() const {
    return is_predator_;
}

void Agent::setPredator(bool v) {
    is_predator_ = v;
}

}  // namespace neuro
