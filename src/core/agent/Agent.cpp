#include "core/agent/Agent.h"

#include "core/interfaces/IBrain.h"
#include "core/interfaces/IAgentCapability.h"
#include "core/interfaces/IPerception.h"
#include "core/interfaces/IWorld.h"

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
      age_(0),
      alive_(true),
      fitness_(0.0),
      decision_interval_(decision_interval),
      next_decision_tick_(0),
      food_eaten_this_tick_(0.0f),
      discovered_new_cell_this_tick_(false),
      exploration_grid_size_(exploration_grid_size),
      damage_dealt_(0.0f),
      kills_(0),
      infected_(false),
      infection_progress_(0.0f),
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
Tick Agent::age() const { return age_; }
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
Tick Agent::nextDecisionTick() const { return next_decision_tick_; }
void Agent::setNextDecisionTick(Tick tick) { next_decision_tick_ = tick; }
bool Agent::shouldDecide(Tick current_tick) const {
    return current_tick >= next_decision_tick_;
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
    discovered_new_cell_this_tick_ = false;
}

void Agent::step(IWorld& world, float dt) {
    if (!alive_) return;

    if (shouldDecide(world.currentTick())) {
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

        auto output_span = brain_->forward(inputs, dt);
        last_outputs_.assign(output_span.begin(), output_span.end());
        next_decision_tick_ = world.currentTick() + static_cast<Tick>(decision_interval_);
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

void Agent::incrementAge() {
    ++age_;
}

void Agent::noteFoodEaten(float amount) {
    (void)amount;
    food_eaten_this_tick_ += 1.0f;
}

void Agent::noteExplorationCell(std::size_t cell_index) {
    auto [_, inserted] = visited_cells_.insert(cell_index);
    discovered_new_cell_this_tick_ = discovered_new_cell_this_tick_ || inserted;
}

float Agent::foodEatenThisTick() const {
    return food_eaten_this_tick_;
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
    if (!v) {
        infection_progress_ = 0.0f;
    }
}

float Agent::infectionProgress() const {
    return infection_progress_;
}

void Agent::advanceInfection(float dt) {
    infection_progress_ += dt;
}

}  // namespace neuro
