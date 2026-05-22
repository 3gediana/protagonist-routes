#include "core/capability/PredatorHuntCapability.h"
#include "core/agent/Agent.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/pathogen/Pathogen.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"
#include "core/world/layers/TimeOfDayLayer.h"
#include "core/world/layers/terrain/TerrainLayer.h"

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
    hunt_attempts.fetch_add(1, std::memory_order_relaxed);

    auto* concrete = dynamic_cast<Agent*>(&agent);
    if (concrete == nullptr) return;

    auto* agent_layer = world.getLayer<AgentLayer>();
    if (agent_layer == nullptr) return;

    concrete->setEnergy(concrete->energy() - cost_ * dt);

    // L2.1 v2 (SPECIES_DIFFERENTIATION_SPEC.md): apex digest satiety
    // and ambush stealth strike. inDigestSatiety() drops attack power
    // sharply (apex sleeping it off). Stealth attackers get a one-shot
    // 2x damage strike on the first hit out of stealth, then exit
    // stealth.
    const SimTimeSeconds now = world.currentTimeSeconds();
    const bool in_satiety = concrete->inDigestSatiety(now);
    const float satiety_mult = in_satiety ? 0.2f : 1.0f;
    const bool stealth_strike = concrete->isStealth();

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

        auto* other_concrete = dynamic_cast<Agent*>(other);
        if (other_concrete == nullptr) continue;

        if (other_concrete->isPredator()) continue;

        const float dx = agent.position().x - other->position().x;
        const float dy = agent.position().y - other->position().y;
        const float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < hunt_radius_) {
            const bool prey_infected = other_concrete->infected();
            const float base_damage = prey_infected ? damage_ * 0.5f : damage_;
            const float base_gain = prey_infected ? energy_gain_ * 0.3f : energy_gain_;

            // Cave shelter: prey in caves take 70% less damage
            float shelter_mult = 1.0f;
            const auto* terrain = world.getLayer<TerrainLayer>();
            if (terrain && terrain->inCave(other->position().x, other->position().y)) {
                shelter_mult = 0.3f;
            }

            // L2.1 v2: stealth one-shot 2x damage. Note dt is per-tick
            // (sub-step), so this multiplies the per-tick damage of the
            // tick the strike lands on, not the entire window. Combined
            // with leopard's already-high base damage (14) this gives
            // the burst that breaks LargeHerbivore threshold solo.
            const float stealth_mult = stealth_strike ? 2.0f : 1.0f;

            const float actual_damage = base_damage * time_damage_mult * shelter_mult
                                        * stealth_mult * satiety_mult * dt;
            float actual_gain = base_gain * time_damage_mult * satiety_mult * dt;
            actual_gain *= 0.85f + 0.3f * concrete->dietPreference();

            // L2.1 v2: scavenger bonus on low-vitality prey. Prey is
            // "low-vitality" if its energy is below 25% of its damage
            // threshold (i.e. close to the kill burst threshold). For
            // prey without a damage_threshold set we fall back to 25%
            // of 100 energy. scavenge_efficiency > 1 boosts gain on
            // dying prey, giving the scavenger species its "wait for
            // a kill, then feast" niche even before L2.3 corpse
            // resource channel lands.
            const float scav_eff = concrete->scavengeEfficiency();
            if (scav_eff != 1.0f) {
                const float low_vit_floor = other_concrete->damageThreshold() > 0.0f
                                                ? other_concrete->damageThreshold() * 0.25f
                                                : 25.0f;
                if (other->energy() < low_vit_floor) {
                    actual_gain *= scav_eff;
                } else if (scav_eff > 1.0f) {
                    // Specialist scavenger taxes itself on healthy prey.
                    actual_gain *= (1.0f / scav_eff);
                }
            }

            // L2.1 v2: damage threshold gate. When prey has a non-zero
            // damage_threshold, attackers accumulate hits in a sliding
            // window; only when the window total >= threshold does the
            // prey actually take the burst as energy loss. Single
            // attacker with damage < threshold cannot break it alone.
            constexpr SimTimeSeconds kThreatWindow = 1.0;
            bool kill_burst_landed = false;
            if (other_concrete->damageThreshold() > 0.0f) {
                other_concrete->accumulateThreatDamage(agent.id(), actual_damage, now);
                const float window_total = other_concrete->threatDamageInWindow(now, kThreatWindow);
                if (window_total >= other_concrete->damageThreshold()) {
                    other->setEnergy(other->energy() - window_total);
                    other_concrete->clearThreatDamageWindow();
                    kill_burst_landed = true;
                }
            } else {
                // Legacy direct-damage path (fragile prey, predator-on-predator).
                other->setEnergy(other->energy() - actual_damage);
            }

            concrete->setEnergy(concrete->energy() + actual_gain);
            world.eventBus().publish(events::Event{
                events::EventType::AgentAttacked,
                events::AgentAttacked{agent.id(), other->id(), actual_damage},
                world.currentTick(),
                world.currentTimeSeconds()});
            concrete->noteDamageDealt(actual_damage);
            if (actual_gain > 0.0f) {
                concrete->noteMeatFood(actual_gain);
            }

            // Stealth dissolves on the first strike out of stealth.
            if (stealth_strike) {
                concrete->setStealth(false);
                concrete->setStationarySeconds(0.0);
            }

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
                hunt_successes.fetch_add(1, std::memory_order_relaxed);
                // L2.1 v2: apex digest satiety. After landing a kill on
                // threshold-class prey (large herbivore), apex enters a
                // 60s nerf window. Triggered by kill-burst path so a
                // pure-luck-of-cooperation kill also triggers (avoids
                // over-tuning to "apex damage >= threshold solo").
                if (concrete->dietPreference() > 0.85f && kill_burst_landed
                    && other_concrete->damageThreshold() >= 10.0f) {
                    concrete->setDigestSatietyUntilSeconds(now + 60.0);
                }
            }
        }
    }
}

const char* PredatorHuntCapability::name() const {
    return "predator_hunt";
}

}  // namespace neuro
