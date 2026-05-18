#include "core/capability/AttackCapability.h"

#include "core/agent/Agent.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"

#include <cmath>

namespace neuro {

AttackCapability::AttackCapability(float attack_range,
                                   float damage_factor,
                                   float energy_transfer_rate,
                                   float attack_cost,
                                   float attack_threshold)
    : attack_range_(attack_range),
      damage_factor_(damage_factor),
      energy_transfer_rate_(energy_transfer_rate),
      attack_cost_(attack_cost),
      attack_threshold_(attack_threshold) {}

std::size_t AttackCapability::consumedOutputs() const {
    return 3;
}

void AttackCapability::apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) {
    if (outputs.size() != 3) return;

    const float intensity = outputs[0];
    if (intensity < attack_threshold_) return;

    const float attack_dx = outputs[1];
    const float attack_dy = outputs[2];
    const float dir_len = std::sqrt(attack_dx * attack_dx + attack_dy * attack_dy);
    if (dir_len < 0.01f) return;

    const Vec2 attack_dir{attack_dx / dir_len, attack_dy / dir_len};
    const auto& pos = agent.position();

    auto* agent_layer = world.getLayer<AgentLayer>();
    if (agent_layer == nullptr) return;

    const float damage = intensity * damage_factor_ * dt;
    float total_damage_dealt = 0.0f;

    for (auto* other : agent_layer->agents()) {
        if (other->id() == agent.id()) continue;
        if (!other->alive()) continue;

        const auto& other_pos = other->position();
        const Vec2 diff{other_pos.x - pos.x, other_pos.y - pos.y};
        const float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);
        if (dist > attack_range_) continue;

        const Vec2 dir_to_other{diff.x / (dist + 0.001f), diff.y / (dist + 0.001f)};
        const float dot = attack_dir.x * dir_to_other.x + attack_dir.y * dir_to_other.y;
        if (dot < 0.3f) continue;

        const float prev_energy = other->energy();
        other->setEnergy(prev_energy - damage);
        total_damage_dealt += damage;

        const float gained = damage * energy_transfer_rate_;
        agent.setEnergy(agent.energy() + gained);

        world.eventBus().publish(events::Event{
            events::EventType::AgentAttacked,
            events::AgentAttacked{agent.id(), other->id(), damage},
            world.currentTick()});

        if (auto* attacker = dynamic_cast<Agent*>(&agent)) {
            attacker->noteDamageDealt(damage);
        }

        if (other->energy() <= 0.0f && prev_energy > 0.0f) {
            if (auto* attacker = dynamic_cast<Agent*>(&agent)) {
                attacker->noteKill();
            }
        }
    }

    if (total_damage_dealt > 0.0f) {
        agent.setEnergy(agent.energy() - attack_cost_ * dt);
    }
}

const char* AttackCapability::name() const {
    return "attack";
}

}  // namespace neuro
