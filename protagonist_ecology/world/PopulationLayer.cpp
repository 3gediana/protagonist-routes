#include "world/PopulationLayer.h"

#include "core/world/layers/AgentLayer.h"
#include "core/world/World2D.h"
#include "core/interfaces/IGenome.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <unordered_map>
#include <utility>

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
    born_children_.clear();
}

std::vector<BornChild> PopulationLayer::drainBornChildren() {
    std::vector<BornChild> out;
    out.swap(born_children_);
    return out;
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
                a->markDeathCounted();
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
                    a->markDeathCounted();
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
    // Predation, Disease, etc.). After L4 #3 all kill paths route through
    // Agent::killWithCause, so death_cause_ is populated. We tally each
    // newly-dead agent exactly once via Agent::death_counted_.
    for (Agent* a : agents) {
        if (a == nullptr) continue;
        if (a->alive()) continue;
        if (a->deathCounted()) continue;
        const auto cause = a->deathCause();
        // The OldAge / Disease branches above already incremented for
        // their own kills (and called markDeathCounted via this loop on
        // the next pass), but we still mark them here so subsequent
        // ticks don't re-enter — and we don't double-tally because we
        // checked deathCounted() up front.
        ++total_deaths_;
        if (static_cast<std::size_t>(cause) < deaths_by_cause_.size()) {
            ++deaths_by_cause_[static_cast<std::size_t>(cause)];
        }
        a->markDeathCounted();
    }

    // Phase 3: reproduction. Throttle to once per coeffs_.reproduction_check_interval.
    const bool should_check = (sim_time_ - last_check_time_) >= coeffs_.reproduction_check_interval;
    if (!should_check) return;
    last_check_time_ = sim_time_;

    // Population guardrail.
    if (agent_layer->livingCount() >= coeffs_.max_living_agents) return;

    // Build a list of eligible parents (adults, energy>=min, cooldown=0,
    // alive). Group by archetype_tag — Agent::speciesId() in this route is
    // kInvalidSpeciesId for every background agent, so using it as the
    // pairing key would silently mate-across-species and crash inside
    // Agent::applyCapabilities later (brain.output_dim is parent_a's, but
    // capabilities are rebuilt from parent_a.archetypeTag()'s wiring).
    // Archetype tag IS set per founder (stampLifecycleAndLineage), so it
    // is the authoritative same-species predicate here.
    struct Candidate {
        Agent* agent;
        std::uint8_t archetype_tag;
    };
    std::vector<Candidate> candidates;
    candidates.reserve(agents.size());
    for (Agent* a : agents) {
        if (a == nullptr) continue;
        if (!a->alive()) continue;
        if (a->lifecycleStage() != Agent::LifecycleStage::Adult) continue;
        if (a->energy() < coeffs_.reproduction_min_energy) continue;
        if (a->reproductionCooldown() > 0.0f) continue;
        // Protagonist breeding goes through cross-episode evolution; we
        // never produce protagonist children here. Skipping protagonists
        // at the candidate-building stage keeps the O(N^2) pair search
        // bounded to background species only.
        if (a->archetypeTag() == 0) continue;
        candidates.push_back({a, a->archetypeTag()});
    }
    if (candidates.size() < 2) return;

    // Shuffle for fair pairing.
    std::shuffle(candidates.begin(), candidates.end(), rng_);

    const float r2 = coeffs_.reproduction_radius * coeffs_.reproduction_radius;
    std::vector<bool> used(candidates.size(), false);

    // L4 #4 (Final Blueprint) perf: spatial bucketing on archetype × cell.
    // Without this, every reproduction check was O(N²) over candidates;
    // with 65→200+ adults and check_interval=10s the cost is small per
    // tick but grows quadratically. Bucket key = (archetype_tag, cell_x,
    // cell_y) with cell_size = reproduction_radius so any pair within
    // radius_r is in the same cell or an adjacent one. We only walk the
    // 3×3 neighbourhood of each candidate's cell, bounding work to O(N).
    const float cell_size = std::max(1.0f, coeffs_.reproduction_radius);
    struct CellKey { std::uint8_t archetype; int cx; int cy; };
    struct CellKeyHash {
        std::size_t operator()(const CellKey& k) const noexcept {
            return (static_cast<std::size_t>(k.archetype) * 73856093u)
                 ^ (static_cast<std::size_t>(k.cx) * 19349663u)
                 ^ (static_cast<std::size_t>(k.cy) * 83492791u);
        }
    };
    struct CellKeyEq {
        bool operator()(const CellKey& a, const CellKey& b) const noexcept {
            return a.archetype == b.archetype && a.cx == b.cx && a.cy == b.cy;
        }
    };
    auto cell_for = [&](const Vec2& p, std::uint8_t arch) {
        return CellKey{arch,
                       static_cast<int>(std::floor(p.x / cell_size)),
                       static_cast<int>(std::floor(p.y / cell_size))};
    };
    std::unordered_map<CellKey, std::vector<std::size_t>, CellKeyHash, CellKeyEq> buckets;
    buckets.reserve(candidates.size());
    for (std::size_t k = 0; k < candidates.size(); ++k) {
        buckets[cell_for(candidates[k].agent->position(), candidates[k].archetype_tag)].push_back(k);
    }

    std::vector<std::unique_ptr<Agent>> new_agents;

    for (std::size_t i = 0; i < candidates.size(); ++i) {
        if (used[i]) continue;
        if (agent_layer->livingCount() + new_agents.size() >= coeffs_.max_living_agents) break;
        // Layer 4 perf throttle: amortize NeatBrain reconstruction cost
        // across multiple reproduction ticks instead of stalling one tick
        // for many simultaneous births.
        if (new_agents.size() >= coeffs_.max_spawns_per_tick) break;

        // Walk only the 3x3 neighbourhood of candidate i's cell.
        const auto base = cell_for(candidates[i].agent->position(), candidates[i].archetype_tag);
        bool paired = false;
        for (int dy = -1; dy <= 1 && !paired; ++dy) {
            for (int dx = -1; dx <= 1 && !paired; ++dx) {
                const CellKey neighbour{candidates[i].archetype_tag, base.cx + dx, base.cy + dy};
                auto it = buckets.find(neighbour);
                if (it == buckets.end()) continue;
                for (const std::size_t j : it->second) {
                    if (j <= i) continue;       // preserve i<j semantics
                    if (used[j]) continue;
                    if (candidates[i].archetype_tag != candidates[j].archetype_tag) continue;
                    const Vec2 pa = candidates[i].agent->position();
                    const Vec2 pb = candidates[j].agent->position();
                    const float ddx = pa.x - pb.x;
                    const float ddy = pa.y - pb.y;
                    if (ddx * ddx + ddy * ddy > r2) continue;
                    // Reuse the original birth path by re-entering the
                    // existing inner-loop body via a goto-substitute:
                    // duplicate-spawn-logic block below. Cleaner than
                    // restructuring control flow with extra captures.
                    // (We rely on j being a valid index into candidates.)
                    (void)pa; (void)pb;
                    paired = true;
                    // Pair found. Allocate IDs.
                    const AgentId new_aid{next_agent_id_++};
                    const GenomeId new_gid{next_genome_id_++};
                    const std::uint64_t new_lineage = next_lineage_id_++;
                    auto child = spawn_cb_(*candidates[i].agent,
                                           *candidates[j].agent,
                                           new_aid, new_gid);
                    if (child == nullptr) {
                        used[j] = true;
                        paired = false;
                        continue;
                    }
                    child->setLineageId(new_lineage);
                    child->setParentLineages(candidates[i].agent->lineageId(),
                                             candidates[j].agent->lineageId());
                    child->setBirthGeneration(std::max(candidates[i].agent->birthGeneration(),
                                                       candidates[j].agent->birthGeneration()) + 1);
                    child->setReproductionCooldown(coeffs_.juvenile_initial_cooldown_sec);
                    child->setEnergy(coeffs_.juvenile_initial_energy);
                    candidates[i].agent->setEnergy(candidates[i].agent->energy() - coeffs_.reproduction_energy_cost);
                    candidates[j].agent->setEnergy(candidates[j].agent->energy() - coeffs_.reproduction_energy_cost);
                    candidates[i].agent->setReproductionCooldown(coeffs_.reproduction_cooldown_sec);
                    candidates[j].agent->setReproductionCooldown(coeffs_.reproduction_cooldown_sec);
                    candidates[i].agent->incrementOffspringCount();
                    candidates[j].agent->incrementOffspringCount();
                    used[i] = used[j] = true;
                    try {
                        if (child->genomeArchive() != nullptr) {
                            auto cloned = child->genomeArchive()->clone();
                            if (cloned != nullptr) {
                                born_children_.push_back(BornChild{
                                    child->archetypeTag(), new_gid, new_aid, std::move(cloned)});
                            }
                        }
                    } catch (...) {
                        spdlog::error("PopulationLayer born-child clone failed");
                    }
                    new_agents.push_back(std::move(child));
                    ++total_births_;
                    break;
                }
            }
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
