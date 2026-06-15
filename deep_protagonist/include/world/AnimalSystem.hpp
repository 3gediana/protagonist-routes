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
    // Persistence-hunting stamina: prey can only sprint at panic_speed for
    // flee_sprint seconds before being winded, then must rest (flee_rest > 0,
    // stopped) so a slower tracking agent can close the gap for a kill.
    float flee_sprint = 1.2f;
    float flee_rest   = 0.0f;
    // D-126 cumulative fatigue. Unlike flee_sprint (which the old code reset to
    // full the instant the prey slipped past FLEE_TRIGGER -- so a 6 m/s prey
    // outrunning a 3 m/s agent NEVER stayed engaged long enough to get winded,
    // and the rest window never opened), flee_stamina DRAINS while fleeing and
    // only refills SLOWLY when safe. Sustained pursuit therefore accumulates
    // fatigue across brief escapes until the prey collapses into a forced rest
    // -- the authentic persistence hunt. Seconds of sprint left before collapse.
    float flee_stamina = 3.0f;
    // Marked by TameSystem: wolves should not pick this prey, and the
    // animal stays close to the agent via TameSystem::apply_follow().
    bool  is_tamed = false;
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
    //
    // Optional `agent_perception_scale` and `agent_speed_scale` reduce
    // how aggressively predators react to the agent (e.g. when the agent
    // is sheltered: scale=0.3 means wolves only see them at 30% of
    // normal range and chase at 30% of normal speed).
    void tick(const World& world,
              float agent_x, float agent_y, float agent_z,
              float dt,
              float night_factor = 0.0f,
              float agent_perception_scale = 1.0f,
              float agent_speed_scale = 1.0f);

    const std::vector<AnimalInstance>& animals() const { return animals_; }
    // Mutable access: used at world-build to cluster a few fish into the
    // water nearest spawn (decision_blueprint_world_coherence §4). Fish are
    // out of the agent observation, so this does not churn the PPO obs.
    std::vector<AnimalInstance>& animals_mut() { return animals_; }

    // Index of the closest animal of given kind, or -1
    int closest_to(float x, float y, AnimalKind kind, float max_dist = 1e6f) const;

    // Damage (e.g. wolf bite on agent calls back into VitalSystem in main).
    // Returns true if the wolf successfully bit (within bite_radius).
    bool wolf_attack_agent(int wolf_idx, float agent_x, float agent_y,
                           float bite_radius = 1.6f) const;

    // Mark dead so it stops moving and rendering pulls a "dead" colour.
    void kill(int idx);

    // Steer the animal at `idx` toward (tx, ty) at `speed` m/s, for one
    // tick. Used by TameSystem so tamed followers stay near the agent.
    void steer_toward(int idx, float tx, float ty, float speed, float dt);

    // Flag an animal as tamed (or untag it). Tamed animals are skipped
    // by wolf prey scans so the agent's followers don't get eaten.
    void mark_tamed(int idx, bool yes);

    struct AttackResult {
        int   index   = -1;     // index of the animal hit; -1 = miss
        bool  killed  = false;  // health dropped to 0 this tick
    };

    // Agent-initiated attack. Picks the closest live animal within `reach`
    // metres in front of the agent (a 90-degree forward arc relative to
    // `facing_yaw`). Subtracts `damage` from its health and, if the kill
    // happens this tick, sets state=Dead.
    AttackResult agent_attack(float agent_x, float agent_y,
                              float facing_yaw, float reach,
                              float damage);

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
    void tick_wolf(AnimalInstance& a, int self_idx, const World& world,
                   float ax, float ay, float dt, float night_factor,
                   float agent_perception_scale,
                   float agent_speed_scale);
    void tick_fish(AnimalInstance& a, const World& world, float dt);
    void tick_eagle(AnimalInstance& a, const World& world, float dt);

    void integrate_with_terrain(AnimalInstance& a, const World& world, float dt);
};

}  // namespace dp::world
