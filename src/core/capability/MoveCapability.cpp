#include "core/capability/MoveCapability.h"

#include "core/agent/Agent.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/terrain/TerrainLayer.h"
#include "core/world/layers/TimeOfDayLayer.h"
#include "core/world/layers/SeasonLayer.h"
#include "core/world/layers/WeatherLayer.h"
#include "core/world/layers/object/ObjectLayer.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace neuro {

MoveCapability::MoveCapability(float max_speed,
                               float decision_cost,
                               float movement_cost_factor,
                               std::size_t exploration_grid_size,
                               bool aerial)
    : max_speed_(max_speed),
      decision_cost_(decision_cost),
      movement_cost_factor_(movement_cost_factor),
      exploration_grid_size_(exploration_grid_size),
      aerial_(aerial) {}

std::size_t MoveCapability::consumedOutputs() const {
    return 2;
}

void MoveCapability::apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) {
    if (outputs.size() != 2) {
        throw std::invalid_argument("MoveCapability expects 2 outputs");
    }

    Vec2 direction{outputs[0], outputs[1]};
    // Sanitize NaN/Inf from neural network outputs
    if (!std::isfinite(direction.x)) direction.x = 0.0f;
    if (!std::isfinite(direction.y)) direction.y = 0.0f;
    const float norm = direction.length();
    if (norm > 1.0f) {
        direction /= norm;
    }

    const Vec2 previous_position = agent.position();
    const float previous_energy = agent.energy();

    Vec2 velocity = direction * max_speed_;

    auto* concrete_agent = dynamic_cast<Agent*>(&agent);
    if (concrete_agent && concrete_agent->infected()) {
        velocity = velocity * 0.6f;
    }

    // L2.7 v13 Detail #2 (carrying slowdown): the protagonist used to
    // walk at full speed while carrying a heavy log or stone. Now
    // a normal carried object multiplies speed by 0.9, a heavy one
    // (carriers_required > 1) by 0.7. Forces a meaningful trade-off
    // between "grab a stone for melee" and "stay fast for fleeing".
    if (concrete_agent != nullptr && concrete_agent->isCarrying()) {
        if (auto* objects = world.getLayer<ObjectLayer>(); objects != nullptr) {
            if (const auto* carried = objects->findById(concrete_agent->carriedObject()); carried != nullptr) {
                if (carried->heavy || carried->carriers_required > 1) {
                    velocity = velocity * 0.7f;
                } else {
                    velocity = velocity * 0.9f;
                }
            }
        }
    }

    auto* terrain = world.getLayer<TerrainLayer>();
    float terrain_cost_factor = 1.0f;

    // Day/night: darkness reduces movement speed — species-dependent
    // Predators: nocturnal (night speed 0.85x), Prey: diurnal (night speed 0.45x)
    const auto* tod = world.getLayer<TimeOfDayLayer>();
    if (tod != nullptr) {
        const float light = tod->lightLevel();
        const bool is_predator = concrete_agent && concrete_agent->isPredator();
        // Predators: min speed 0.85x at darkest; Prey: min speed 0.45x at darkest
        const float min_factor = is_predator ? 0.85f : 0.45f;
        const float light_factor = min_factor + (1.0f - min_factor) * light;
        velocity = velocity * light_factor;
    }

    if (terrain != nullptr && !aerial_) {
        // Ground-bound agents are slowed by slope and benefit from cave shelter.
        const Vec2 slope = terrain->slopeAt(previous_position.x, previous_position.y);
        const float slope_mag = std::sqrt(slope.x * slope.x + slope.y * slope.y);
        const float terrain_factor = std::max(terrain->minVelocityFactor(),
                                              1.0f / (1.0f + slope_mag * terrain->slopeResistance()));
        velocity = velocity * terrain_factor;

        const float uphill = velocity.x * slope.x + velocity.y * slope.y;
        if (uphill > 0.0f) {
            terrain_cost_factor += std::abs(uphill) * terrain->slopeResistance();
        }

        if (terrain->inCave(previous_position.x, previous_position.y)) {
            float shelter = terrain->caveShelterFactor();
            // Night shelter bonus: caves are more valuable at night (0.3x cost vs 0.5x)
            if (tod && tod->isNight()) {
                shelter *= 0.6f;
            }
            terrain_cost_factor *= shelter;
        }
    } else if (terrain != nullptr && aerial_) {
        // Aerial agents (e.g. vultures) glide over terrain: small base cost surcharge
        // representing flight metabolism, but no slope penalty.
        terrain_cost_factor *= 1.15f;
    }

    Vec2 next_position = previous_position + velocity * dt;
    const Vec2 world_size = world.size();
    next_position.x = std::clamp(next_position.x, 0.0f, world_size.x);
    next_position.y = std::clamp(next_position.y, 0.0f, world_size.y);

    agent.setVelocity(velocity);
    agent.setPosition(next_position);

    const float move_cost = (decision_cost_ + movement_cost_factor_ * velocity.length()) * terrain_cost_factor * dt;

    // Season affects metabolism: winter = 1.5x energy cost, summer = 0.9x
    const auto* season = world.getLayer<SeasonLayer>();
    float season_cost = 1.0f;
    if (season) {
        season_cost = season->metabolismFactor();
    }

    // Layer 2 (FINAL_BLUEPRINT.md §2.3): weather affects movement cost.
    // High wind/typhoon: 1.5x / 1.8x (struggling against wind).
    float weather_cost = 1.0f;
    if (const auto* weather = world.getLayer<WeatherLayer>(); weather != nullptr) {
        weather_cost = weather->moveCostMultiplier();
    }

    agent.setEnergy(previous_energy - move_cost * season_cost * weather_cost);

    world.eventBus().publish(events::Event{events::EventType::AgentMoved,
                                           events::AgentMoved{agent.id(), previous_position, next_position},
                                           world.currentTick(),
                                           world.currentTimeSeconds()});
    world.eventBus().publish(events::Event{events::EventType::AgentEnergyChanged,
                                           events::AgentEnergyChanged{agent.id(), previous_energy, agent.energy()},
                                           world.currentTick(),
                                           world.currentTimeSeconds()});

    if (auto* concrete = dynamic_cast<Agent*>(&agent)) {
        const float cell_width = world_size.x / static_cast<float>(exploration_grid_size_);
        const float cell_height = world_size.y / static_cast<float>(exploration_grid_size_);
        const std::size_t cell_x = std::min(exploration_grid_size_ - 1,
                                            static_cast<std::size_t>(next_position.x / (cell_width > 0.0f ? cell_width : 1.0f)));
        const std::size_t cell_y = std::min(exploration_grid_size_ - 1,
                                            static_cast<std::size_t>(next_position.y / (cell_height > 0.0f ? cell_height : 1.0f)));
        concrete->noteExplorationCell(cell_y * exploration_grid_size_ + cell_x);
    }
}

const char* MoveCapability::name() const {
    return "move";
}

}  // namespace neuro
