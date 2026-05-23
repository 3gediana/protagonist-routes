#pragma once

#include "world/World.hpp"

#include <cstdint>
#include <vector>

namespace dp::world {

enum class AnimalKind : uint8_t {
    Rabbit = 0,
    Deer   = 1,
    Wolf   = 2,
    Fish   = 3,
    Eagle  = 4,
};

const char* animal_kind_name(AnimalKind k);

enum class AnimalState : uint8_t {
    Idle,
    Wander,
    Flee,
    Hunt,
    Eat,
    Drink,
    Dead,
};

struct AnimalInstance {
    AnimalKind  kind;
    AnimalState state    = AnimalState::Wander;
    float pos_x = 0.0f, pos_y = 0.0f, pos_z = 0.0f;
    float vel_x = 0.0f, vel_y = 0.0f, vel_z = 0.0f;
    float yaw   = 0.0f;
    float health = 100.0f;
    float state_timer = 0.0f;
    float wander_x = 1.0f, wander_y = 0.0f;
};

// AnimalSystem runs scripted (non-learning) AI for the world's wildlife.
// Animals are simple state machines, intentionally NOT neural - they're the
// "extra cast", not the protagonist.
//
// Threat / prey relationships (S3 baseline):
//   - Wolves hunt the agent and rabbits/deer
//   - Rabbits & Deer flee from the agent and from wolves
//   - Fish wander in water cells (River/Swamp biomes)
//   - Eagles fly above ground (decorative for now)
class AnimalSystem {
public:
    void initialize(uint64_t seed, const World& world,
                    int rabbits = 50, int deer = 20,
                    int wolves = 8, int fish = 30,
                    int eagles = 10);

    // Step animal AI one frame. Pass the agent's current foot position so
    // wolves/deer/rabbits can react to it. `night_factor` is in [0,1]
    // and is used to amp up nocturnal predators (wolves get +20% speed
    // and a wider perception range at night).
    void tick(const World& world,
              float agent_x, float agent_y, float agent_z,
              float dt,
              float night_factor = 0.0f);

    const std::vector<AnimalInstance>& animals() const { return animals_; }

    // Index of the closest animal of given kind, or -1
    int closest_to(float x, float y, AnimalKind kind, float max_dist = 1e6f) const;

    // Damage (e.g. wolf bite on agent calls back into VitalSystem in main).
    // Returns true if the wolf successfully bit (within bite_radius).
    bool wolf_attack_agent(int wolf_idx, float agent_x, float agent_y,
                           float bite_radius = 1.6f) const;

    // Mark dead so it stops moving and rendering pulls a "dead" colour.
    void kill(int idx);

private:
    std::vector<AnimalInstance> animals_;
    uint64_t rng_state_ = 1;

    float frand();
    void  spawn(AnimalInstance& a, AnimalKind k, const World& world);

    // Per-kind tick helpers
    void tick_rabbit(AnimalInstance& a, const World& world,
                     float ax, float ay, float dt);
    void tick_deer(AnimalInstance& a, const World& world,
                   float ax, float ay, float dt);
    void tick_wolf(AnimalInstance& a, const World& world,
                   float ax, float ay, float dt, float night_factor);
    void tick_fish(AnimalInstance& a, const World& world, float dt);
    void tick_eagle(AnimalInstance& a, const World& world, float dt);

    void integrate_with_terrain(AnimalInstance& a, const World& world, float dt);
};

}  // namespace dp::world
