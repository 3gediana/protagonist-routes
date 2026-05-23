#include "world/PopulationLayer.h"

#include "core/world/layers/AgentLayer.h"
#include "core/world/World2D.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <random>

namespace neuro::routes::protagonist {

PopulationLayer::PopulationLayer(Coefficients coeffs, SpawnCallback spawn_cb)
    : coeffs_(coeffs),
      spawn_cb_(std::move(spawn_cb)),
      rng_(coeffs.seed == 0 ? std::random_device{}() : coeffs.seed) {}

void PopulationLayer::onAttach(IWorld& /*world*/) {
    sim_time_ = 0.0;
    last_check_time_ = -1.0e9;
    deaths_by_cause_.fill(0);
    total_births_ = 0;
    total_deaths_ = 0;
}

void PopulationLayer::resetStatistics() {
    sim_time_ = 0.0;
    last_check_time_ = -1.0e9;
    deaths_by_cause_.fill(0);
    total_births_ = 0;
    total_deaths_ = 0;
}

void PopulationLayer::setNextIds(std::uint64_t agent_id, std::uint64_t genome_id, std::uint64_t lineage_id) {
    next_agent_id_ = agent_id;
    next_genome_id_ = genome_id;
    next_lineage_id_ = lineage_id;
}

std::size_t PopulationLayer::deathsByCause(Agent::DeathCause c) const {
    const auto idx = static_cast<std::size_t>(c);
    return idx < deaths_by_cause_.size() ? deaths_by_cause_[idx] : 0;
}

void PopulationLayer::tick(IWorld& world, float dt) {
    sim_time_ += static_cast<SimTimeSeconds>(dt);

    auto* agent_layer = world.getLayer<AgentLayer>();
    if (agent_layer == nullptr) return;

    auto agents = agent_layer->agents();

    // Phase 1: per-agent updates (cooldown decay, age-related mortality).
    try {
        for (Agent* a : agents) {
            if (a == nullptr) continue;
            if (!a->alive()) continue;

            a->decrementReproductionCooldown(dt);

            const SimTimeSeconds max_age = a->maxAgeSeconds();
            if (max_age > 0.0 && a->ageSeconds() >= max_age) {
                a->killWithCause(Agent::DeathCause::OldAge);
                ++total_deaths_;
                ++deaths_by_cause_[static_cast<std::size_t>(Agent::DeathCause::OldAge)];
                continue;
            }

            // Elder-phase frailty: small per-tick chance of dying from
            // "Disease" (proxy for accumulated damage / immune decline).
            if (a->lifecycleStage() == Agent::LifecycleStage::Elder
                && coeffs_.elder_extra_mortality_per_sec > 0.0f) {
                std::uniform_real_distribution<float> u(0.0f, 1.0f);
                const float p = coeffs_.elder_extra_mortality_per_sec * dt;
                if (u(rng_) < p) {
                    a->killWithCause(Agent::DeathCause::Disease);
                    ++total_deaths_;
                    ++deaths_by_cause_[static_cast<std::size_t>(Agent::DeathCause::Disease)];
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("PopulationLayer phase1 exception: {}", e.what());
        throw;
    } catch (...) {
        spdlog::error("PopulationLayer phase1 unknown exception");
        throw;
    }

    // Phase 2: classify deaths from upstream layers (Starvation, Combat,
    // etc.). When other layers call agent->kill(reason_str), the
    // alive_=false but death_cause_ stays Unknown. We can't easily
    // attribute those here without the upstream layer cooperating, so
    // for now we just leave them as Unknown and count them.
    // (Future: SurvivalStatusLayer can call killWithCause directly.)
    for (Agent* a : agents) {
        if (a == nullptr) continue;
        if (a->alive()) continue;
        // Already counted above (OldAge / Disease). Anything else
        // counted via callbacks elsewhere — for now we don't double-count.
    }

    // Phase 3: reproduction. Throttle to once per coeffs_.reproduction_check_interval.
    const bool should_check = (sim_time_ - last_check_time_) >= coeffs_.reproduction_check_interval;
    if (!should_check) return;
    last_check_time_ = sim_time_;

    // Population guardrail.
    if (agent_layer->livingCount() >= coeffs_.max_living_agents) return;

    // Build a list of eligible parents (adults, energy>=min, cooldown=0,
    // alive). Group by species_id.
    struct Candidate {
        Agent* agent;
        SpeciesId species;
    };
    std::vector<Candidate> candidates;
    candidates.reserve(agents.size());
    for (Agent* a : agents) {
        if (a == nullptr) continue;
        if (!a->alive()) continue;
        if (a->lifecycleStage() != Agent::LifecycleStage::Adult) continue;
        if (a->energy() < coeffs_.reproduction_min_energy) continue;
        if (a->reproductionCooldown() > 0.0f) continue;
        candidates.push_back({a, a->speciesId()});
    }
    if (candidates.size() < 2) return;

    // Shuffle for fair pairing.
    std::shuffle(candidates.begin(), candidates.end(), rng_);

    const float r2 = coeffs_.reproduction_radius * coeffs_.reproduction_radius;
    std::vector<bool> used(candidates.size(), false);

    std::vector<std::unique_ptr<Agent>> new_agents;

    for (std::size_t i = 0; i < candidates.size(); ++i) {
        if (used[i]) continue;
        if (agent_layer->livingCount() + new_agents.size() >= coeffs_.max_living_agents) break;
        // Layer 4 perf throttle: amortize NeatBrain reconstruction cost
        // across multiple reproduction ticks instead of stalling one tick
        // for many simultaneous births.
        if (new_agents.size() >= coeffs_.max_spawns_per_tick) break;
        for (std::size_t j = i + 1; j < candidates.size(); ++j) {
            if (used[j]) continue;
            if (candidates[i].species.value != candidates[j].species.value) continue;
            const Vec2 pa = candidates[i].agent->position();
            const Vec2 pb = candidates[j].agent->position();
            const float dx = pa.x - pb.x;
            const float dy = pa.y - pb.y;
            if (dx * dx + dy * dy > r2) continue;

            // Pair found. Allocate IDs.
            const AgentId new_aid{next_agent_id_++};
            const GenomeId new_gid{next_genome_id_++};
            const std::uint64_t new_lineage = next_lineage_id_++;

            auto child = spawn_cb_(*candidates[i].agent,
                                   *candidates[j].agent,
                                   new_aid, new_gid);
            if (child == nullptr) {
                // Spawn callback declined this pair; mark used to avoid
                // re-trying same i.
                used[j] = true;
                continue;
            }

            // Stamp lineage on child.
            child->setLineageId(new_lineage);
            child->setParentLineages(candidates[i].agent->lineageId(),
                                     candidates[j].agent->lineageId());
            child->setBirthGeneration(std::max(candidates[i].agent->birthGeneration(),
                                               candidates[j].agent->birthGeneration()) + 1);
            child->setReproductionCooldown(coeffs_.juvenile_initial_cooldown_sec);
            child->setEnergy(coeffs_.juvenile_initial_energy);

            // Charge parents.
            candidates[i].agent->setEnergy(candidates[i].agent->energy() - coeffs_.reproduction_energy_cost);
            candidates[j].agent->setEnergy(candidates[j].agent->energy() - coeffs_.reproduction_energy_cost);
            candidates[i].agent->setReproductionCooldown(coeffs_.reproduction_cooldown_sec);
            candidates[j].agent->setReproductionCooldown(coeffs_.reproduction_cooldown_sec);
            candidates[i].agent->incrementOffspringCount();
            candidates[j].agent->incrementOffspringCount();

            used[i] = used[j] = true;
            new_agents.push_back(std::move(child));
            ++total_births_;
            break;
        }
    }

    // Register newborns.
    for (auto& child : new_agents) {
        agent_layer->addAgent(std::move(child), world);
    }

    if (!new_agents.empty()) {
        spdlog::info("PopulationLayer: {} births (total_births={}, total_deaths={}, living={})",
                     new_agents.size(), total_births_, total_deaths_, agent_layer->livingCount());
    }
}

}  // namespace neuro::routes::protagonist
