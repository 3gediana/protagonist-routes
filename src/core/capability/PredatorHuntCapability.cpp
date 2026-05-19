#include "core/capability/PredatorHuntCapability.h"
#include "core/agent/Agent.h"
#include "core/pathogen/Pathogen.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"
#include "core/world/layers/TimeOfDayLayer.h"

#include <cmath>
#include <random>

namespace neuro {

PredatorHuntCapability::PredatorHuntCapability(float hunt_radius, float damage, float energy_gain, float cost)
    : hunt_radius_(hunt_radius), damage_(damage), energy_gain_(energy_gain), cost_(cost) {}

std::size_t PredatorHuntCapability::consumedOutputs() const {
    return 1;
}

void PredatorHuntCapability::apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) {
    if (outputs.empty() || outputs[0] < 0.3f) return;

    auto* concrete = dynamic_cast<Agent*>(&agent);
    if (concrete == nullptr) return;

    auto* agent_layer = world.getLayer<AgentLayer>();
    if (agent_layer == nullptr) return;

    concrete->setEnergy(concrete->energy() - cost_ * dt);

    // Crepuscular hunting bonus: predators are most effective at dawn/dusk
    const auto* tod = world.getLayer<TimeOfDayLayer>();
    float time_damage_mult = 1.0f;
    if (tod != nullptr) {
        const float light = tod->lightLevel();
        // Dawn/dusk: light in [0.3, 0.7] → 1.5x damage; full day/night → 1.0x
        const float crepuscular = 1.0f - std::abs(light - 0.5f) * 2.0f;  // 0 at extremes, 1 at 0.5
        time_damage_mult = 1.0f + 0.5f * crepuscular;
    }

    for (auto* other : agent_layer->agents()) {
        if (other == &agent || !other->alive()) continue;

        const auto* other_concrete = dynamic_cast<const Agent*>(other);
        if (other_concrete == nullptr) continue;

        if (other_concrete->isPredator()) continue;

        const float dx = agent.position().x - other->position().x;
        const float dy = agent.position().y - other->position().y;
        const float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < hunt_radius_) {
            const bool prey_infected = other_concrete->infected();
            const float base_damage = prey_infected ? damage_ * 0.5f : damage_;
            const float base_gain = prey_infected ? energy_gain_ * 0.3f : energy_gain_;
            const float actual_damage = base_damage * time_damage_mult * dt;
            const float actual_gain = base_gain * time_damage_mult * dt;

            other->setEnergy(other->energy() - actual_damage);
            concrete->setEnergy(concrete->energy() + actual_gain);
            concrete->noteDamageDealt(actual_damage);

            // Predator can catch disease from infected prey (unless immune to that strain)
            if (prey_infected) {
                const auto* prey_pathogen = other_concrete->pathogen();
                if (prey_pathogen) {
                    // Check strain-based immunity
                    if (concrete->hasImmunityToStrain(prey_pathogen->genome().strain())) {
                        // Already immune to this strain
                    } else {
                        thread_local std::mt19937 pred_rng(std::random_device{}());
                        std::bernoulli_distribution catch_disease(0.3f);
                        if (catch_disease(pred_rng)) {
                            auto child = prey_pathogen->offspring(static_cast<std::uint32_t>(pred_rng()));
                            concrete->setInfected(true);
                            concrete->attachPathogen(std::make_unique<Pathogen>(std::move(child)));
                        }
                    }
                }
            }

            if (other->energy() <= 0.0f) {
                other->kill("predation");
                concrete->noteKill();
            }
        }
    }
}

const char* PredatorHuntCapability::name() const {
    return "predator_hunt";
}

}  // namespace neuro
