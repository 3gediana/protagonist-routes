#include "runtime/BootstrapPopulation.h"

#include "brain/ProtagonistBrain.h"
#include "brain/ProtagonistGenome.h"
#include "capability/CaveHideCapability.h"
#include "capability/ChopTreeCapability.h"
#include "capability/CraftSpearCapability.h"
#include "capability/DrinkWaterCapability.h"
#include "capability/GoalActionAssistCapability.h"
#include "capability/GoalAwareDropCapability.h"
#include "capability/NurseryPickUpCapability.h"
#include "capability/ProtagonistHuntCapability.h"
#include "capability/StealthCapability.h"
#include "capability/ThermalTickCapability.h"
#include "capability/ThrowCarriedObjectCapability.h"
#include "core/agent/Agent.h"
#include "core/brain/fixed/FixedTopologyBrain.h"
#include "core/brain/fixed/FixedTopologyGenome.h"
#include "core/capability/AttackCapability.h"
#include "core/capability/DriveTickCapability.h"
#include "core/capability/EatCapability.h"
#include "core/capability/EmitSignalCapability.h"
#include "core/capability/FleeCapability.h"
#include "core/capability/MoveCapability.h"
#include "core/capability/PredatorHuntCapability.h"
#include "core/brain/neat/NeatBrain.h"
#include "core/brain/neat/NeatGenome.h"
#include "core/perception/InfectionPerception.h"
#include "core/perception/NearestFoodPerception.h"
#include "core/perception/SeasonPerception.h"
#include "core/perception/SelfPerception.h"
#include "core/perception/SignalPerception.h"
#include "core/perception/TerrainPerception.h"
#include "core/perception/TimeOfDayPerception.h"
#include "core/perception/WeatherPerception.h"
#include "perception/AgentSocialPerception.h"
#include "perception/BasePerception.h"
#include "perception/CarriedObjectPerception.h"
#include "perception/DangerPerception.h"
#include "perception/GoalContextPerception.h"
#include "perception/MemoryCognitionPerception.h"
#include "perception/MemoryTrialPerception.h"
#include "perception/ObjectLogisticsPerception.h"
#include "perception/StockpilePerception.h"
#include "perception/SurvivalStatePerception.h"
#include "perception/TaskContextPerception.h"
#include "perception/TreePerception.h"
#include "perception/WaterPerception.h"
#include "perception/WeaponPerception.h"
#include "perception/WorksitePerception.h"
#include "core/interfaces/IGenome.h"
#include "core/world/World2D.h"
#include "core/world/layers/AgentLayer.h"
#include "world/BaseLayer.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <random>
#include <stdexcept>

namespace neuro::routes::protagonist {

namespace {

Vec2 randomPosition(const Vec2& size, std::mt19937& rng) {
    std::uniform_real_distribution<float> x_dist(0.0f, size.x);
    std::uniform_real_distribution<float> y_dist(0.0f, size.y);
    return Vec2{x_dist(rng), y_dist(rng)};
}

Vec2 nurseryPosition(const Vec2& size,
                     const BaseLayer* base_layer,
                     std::mt19937& rng,
                     float radius_multiplier = 0.85f) {
    if (base_layer == nullptr) {
        return randomPosition(size, rng);
    }

    std::uniform_real_distribution<float> angle_dist(0.0f, 6.28318530718f);
    std::uniform_real_distribution<float> radius_dist(0.0f, base_layer->nestRadius() * radius_multiplier);
    const float angle = angle_dist(rng);
    const float radius = radius_dist(rng);
    Vec2 position = base_layer->nestCenter() + Vec2{std::cos(angle) * radius, std::sin(angle) * radius};
    position.x = std::clamp(position.x, 0.0f, size.x);
    position.y = std::clamp(position.y, 0.0f, size.y);
    return position;
}

bool hasBackgroundAgents(const ProtagonistEcologyConfig& config) {
    if (config.background.forager_count > 0 || config.background.predator_count > 0) {
        return true;
    }
    for (const auto& slot : config.background.slots) {
        if (slot.count > 0) return true;
    }
    return false;
}

}  // namespace

std::size_t protagonistSensoryDim(const ProtagonistEcologyConfig& config) {
    const bool has_tree_curriculum = config.tree.enabled && config.objects.enabled && config.base.enabled;
    const bool has_tool_curriculum = config.objects.enabled && config.base.enabled;

    std::size_t sensory_dim = 0;
    sensory_dim += 2;
    sensory_dim += 3;
    sensory_dim += BasePerception::kDim;
    if (config.water.enabled) {
        sensory_dim += WaterPerception::kDim;
    }
    if (config.survival.enabled) {
        sensory_dim += SurvivalStatePerception::kDim;
    }
    if (hasBackgroundAgents(config)) {
        sensory_dim += AgentSocialPerception::kDim;
    }
    if (has_tree_curriculum) {
        sensory_dim += TreePerception::kDim;
    }
    if (has_tool_curriculum) {
        sensory_dim += CarriedObjectPerception::kDim;
        sensory_dim += StockpilePerception::kDim;
        sensory_dim += WeaponPerception::kDim;
    }
    if (config.objects.enabled) {
        sensory_dim += ObjectLogisticsPerception::kDim;
    }
    if (config.campfire.enabled) {
        sensory_dim += DangerPerception::kDim;
    }
    if (config.worksite.enabled) {
        sensory_dim += WorksitePerception::kDim;
    }
    if (config.terrain_enabled) {
        sensory_dim += TerrainPerception::kDim;
    }
    if (config.signal.enabled) {
        sensory_dim += SignalPerception::featureDim(config.signal.channels);
    }
    if (config.memory_trial.enabled) {
        sensory_dim += MemoryTrialPerception::kDim;
    }

    // Memory V2 cognition input
    if (config.protagonist.memory_cognition_enabled) {
        sensory_dim += MemoryCognitionPerception::kDim;
    }

    // L2.6 v12 (PEARL/RL² context-conditioned policy): always reserve
    // 8 onehot inputs for the active task_template index. When
    // task_templates is empty (task_template_idx = -1) all 8 slots are
    // 0, so the brain ignores them. When non-empty, the network can
    // learn template-specific behaviour without needing to delete
    // templates from the pool.
    if (config.task_context_perception_enabled) {
        sensory_dim += TaskContextPerception::kDim;
    }

    // L2.7 v13 (HRL goal-context input): 13-onehot of the protagonist's
    // currently-active GoalType, so the worker policy can specialise
    // to the manager's high-level directive (Build / Hunt / Flee /...).
    if (config.goal_context_perception_enabled) {
        sensory_dim += GoalContextPerception::kDim;
    }

    return sensory_dim;
}

namespace {

std::unique_ptr<IBrain> makeProtagonistBrain(const IGenome& genome,
                                             const ProtagonistEcologyConfig& config,
                                             AgentId id) {
    if (const auto* protagonist_genome = dynamic_cast<const ProtagonistGenome*>(&genome); protagonist_genome != nullptr) {
        return std::make_unique<ProtagonistBrain>(*protagonist_genome, config.compute.protagonist_dense_gpu_enabled);
    }

    return std::make_unique<ProtagonistBrain>(
        protagonistSensoryDim(config),
        config.protagonist.hidden_dim,
        config.protagonist.memory_cells,
        config.protagonist.memory_slots,
        config.protagonist.read_heads,
        config.protagonist.action_dim,
        config.random_seed + static_cast<std::uint32_t>(id.value * 31),
        config.compute.protagonist_dense_gpu_enabled);
}

std::unique_ptr<Agent> makeProtagonistAgent(AgentId id,
                                       GenomeId genome_id,
                                       const ProtagonistGenome* genome,
                                       const ProtagonistEcologyConfig& config,
                                       const BaseLayer* base_layer,
                                       std::mt19937& rng) {
    constexpr SimTimeSeconds kMaxAge = 4000.0;
    constexpr float kInitialEnergy = 120.0f;
    constexpr float kMoveCost = 0.01f;
    constexpr float kMovementFactor = 0.002f;
    constexpr float kEatRadius = 4.5f;
    constexpr float kSignalCost = 0.01f;
    constexpr float kSignalRadius = 16.0f;
    constexpr float kSocialSenseRadius = 24.0f;
    constexpr float kTreeSenseRadius = 30.0f;
    constexpr float kChopRadius = 3.0f;
    constexpr float kThrowSpeed = 7.5f;
    constexpr float kThrowRadius = 14.0f;
    constexpr float kWeaponSenseRadius = 20.0f;
    constexpr float kObjectSenseRadius = 18.0f;
    constexpr float kPickRadius = 5.0f;
    constexpr float kPickCost = 0.012f;
    constexpr float kDropCost = 0.004f;
    constexpr float kDangerSenseRadius = 28.0f;
    constexpr float kWorksiteSenseRadius = 28.0f;
    constexpr float kWaterSenseRadius = 36.0f;
    constexpr float kEatRate = 48.0f;

    const bool has_tree_curriculum = config.tree.enabled && config.objects.enabled && config.base.enabled;
    const bool has_tool_curriculum = config.objects.enabled && config.base.enabled;

    const std::size_t sensory_dim = protagonistSensoryDim(config);

    const std::size_t required_action_dim = 2 + (config.signal.enabled ? config.signal.channels : 0) + (config.water.enabled && config.survival.enabled ? 1 : 0) + (hasBackgroundAgents(config) ? 2 : 0) + (has_tree_curriculum ? 1 : 0) + (has_tool_curriculum ? 3 : 0) + (config.objects.enabled ? 2 : 0);
    if (config.protagonist.action_dim != required_action_dim) {
        throw std::runtime_error("protagonist action_dim does not match configured move/signal capability outputs");
    }

    auto brain = genome != nullptr ? makeProtagonistBrain(*genome, config, id)
                                   : std::make_unique<ProtagonistBrain>(
                                         sensory_dim,
                                         config.protagonist.hidden_dim,
                                         config.protagonist.memory_cells,
                                         config.protagonist.memory_slots,
                                         config.protagonist.read_heads,
                                         config.protagonist.action_dim,
                                         config.random_seed + static_cast<std::uint32_t>(id.value * 31),
                                         config.compute.protagonist_dense_gpu_enabled);

    auto agent = std::make_unique<Agent>(
        id,
        genome_id,
        std::move(brain),
        nurseryPosition(config.world_size, base_layer, rng),
        kInitialEnergy,
        20,
        1.0f);

    agent->addPerception(std::make_unique<SelfPerception>(kInitialEnergy, kMaxAge));
    agent->addPerception(std::make_unique<NearestFoodPerception>());
    agent->addPerception(std::make_unique<BasePerception>());
    if (config.water.enabled) {
        agent->addPerception(std::make_unique<WaterPerception>(
            kWaterSenseRadius,
            config.water.direction_requires_visibility));
    }
    if (config.survival.enabled) {
        agent->addPerception(std::make_unique<SurvivalStatePerception>());
    }
    if (hasBackgroundAgents(config)) {
        agent->addPerception(std::make_unique<AgentSocialPerception>(kSocialSenseRadius));
    }
    if (has_tree_curriculum) {
        agent->addPerception(std::make_unique<TreePerception>(kTreeSenseRadius, kChopRadius));
    }
    if (has_tool_curriculum) {
        agent->addPerception(std::make_unique<CarriedObjectPerception>());
        agent->addPerception(std::make_unique<StockpilePerception>());
        agent->addPerception(std::make_unique<WeaponPerception>(kWeaponSenseRadius));
    }
    if (config.objects.enabled) {
        agent->addPerception(std::make_unique<ObjectLogisticsPerception>(kObjectSenseRadius));
    }
    if (config.campfire.enabled) {
        agent->addPerception(std::make_unique<DangerPerception>(kDangerSenseRadius));
    }
    if (config.worksite.enabled) {
        agent->addPerception(std::make_unique<WorksitePerception>(
            kWorksiteSenseRadius,
            config.worksite.direction_requires_visibility));
    }
    if (config.terrain_enabled) {
        agent->addPerception(std::make_unique<TerrainPerception>(config.terrain.height_scale));
    }
    if (config.signal.enabled) {
        agent->addPerception(std::make_unique<SignalPerception>(config.signal.channels, kSignalRadius));
    }
    if (config.memory_trial.enabled) {
        agent->addPerception(std::make_unique<MemoryTrialPerception>());
    }

    // Memory V2 Cognition Perception
    if (config.protagonist.memory_cognition_enabled) {
        agent->addPerception(std::make_unique<MemoryCognitionPerception>());
    }

    // L2.6 v12 task context perception (PEARL/RL² context-conditioned).
    // Always last so the order matches protagonistSensoryDim().
    if (config.task_context_perception_enabled) {
        agent->addPerception(std::make_unique<TaskContextPerception>());
    }

    // L2.7 v13 goal context perception (HRL worker conditioning).
    if (config.goal_context_perception_enabled) {
        agent->addPerception(std::make_unique<GoalContextPerception>());
    }

    agent->addCapability(std::make_unique<MoveCapability>(2.2f, kMoveCost, kMovementFactor, 20));
    if (config.protagonist.goal_action_assist_enabled) {
        agent->addCapability(std::make_unique<GoalActionAssistCapability>(5.0f, kPickRadius));
    }
    agent->addCapability(std::make_unique<EatCapability>(kEatRadius, kEatRate));
    if (config.signal.enabled) {
        agent->addCapability(std::make_unique<EmitSignalCapability>(
            config.signal.channels,
            kSignalCost,
            config.signal.protagonist_emit_enabled));
    }
    if (config.water.enabled && config.survival.enabled) {
        agent->addCapability(std::make_unique<DrinkWaterCapability>());
    }
    if (hasBackgroundAgents(config)) {
        agent->addCapability(std::make_unique<ProtagonistHuntCapability>(
            5.0f,
            10.0f,
            6.0f,
            0.02f,
            config.protagonist.hunt_enabled));
        agent->addCapability(std::make_unique<FleeCapability>(16.0f, 3.0f, 0.03f));
    }
    if (has_tree_curriculum) {
        agent->addCapability(std::make_unique<ChopTreeCapability>(kChopRadius, 6.0f, 0.02f));
    }
    if (has_tool_curriculum) {
        agent->addCapability(std::make_unique<CraftSpearCapability>(0.025f));
        agent->addCapability(std::make_unique<ThrowCarriedObjectCapability>(kThrowSpeed, kThrowRadius, 10.0f, 18.0f, 0.02f));
    }
    if (config.objects.enabled) {
        agent->addCapability(std::make_unique<NurseryPickUpCapability>(kPickRadius, kPickCost));
        agent->addCapability(std::make_unique<GoalAwareDropCapability>(kDropCost));
    }

    // L2.2 v2: drive tick (last so it sees foodEatenThisTick from
    // EatCapability). consumedOutputs = 0; safe to add unconditionally.
    agent->addCapability(std::make_unique<DriveTickCapability>(kSocialSenseRadius));

    // L1 (Final Blueprint): thermal regulation per R-001. All living
    // entities have core_temperature_; ThermalTickCapability integrates
    // season + day-night + fire warming + metabolic heat each tick.
    agent->addCapability(std::make_unique<ThermalTickCapability>());

    // L2.2 v2: protagonist setpoint = 0 across all 5 drives ("all
    // discomforts are bad"). The species comparison table in spec § 2.1
    // is for background species; protagonist starts uniform-0.
    Agent::DriveSetpoints sp{};
    agent->setDriveSetpoints(sp);

    return agent;
}

std::size_t computeBackgroundSensoryDim(const ProtagonistEcologyConfig& config) {
    std::size_t dim = 0;
    dim += SelfPerception(100.0f, 3000.0).dim();
    dim += NearestFoodPerception().dim();
    dim += AgentSocialPerception::kDim;
    if (config.day_night.enabled) {
        dim += TimeOfDayPerception::kDim;
    }
    if (config.season.enabled) {
        dim += SeasonPerception::kDim;
    }
    // Layer 2 (FINAL_BLUEPRINT.md §2.3): weather perception (6-dim one-hot).
    // MUST match the addPerception ordering in addBackgroundCommonPerceptions
    // below — otherwise brain sensory_dim mismatches actual perception
    // output and we get OOB reads at first forward.
    if (config.weather.enabled) {
        dim += WeatherPerception::kDim;
    }
    if (config.disease.enabled) {
        dim += InfectionPerception::kDim;
    }
    // Background species used to be deaf - they had no SignalPerception, so
    // protagonist warning shouts and inter-prey signaling were invisible to
    // them. Adding it here lets prey react to alarm calls and predators
    // pick up communication-driven prey-aggregation cues, which is exactly
    // the kind of "the world can be understood" that the project aims for.
    if (config.signal.enabled) {
        dim += SignalPerception::featureDim(config.signal.channels);
    }
    return dim;
}

void attachBackgroundCommonSensors(Agent& agent,
                                   const ProtagonistEcologyConfig& config,
                                   float initial_energy) {
    agent.addPerception(std::make_unique<SelfPerception>(initial_energy, 3000.0));
    agent.addPerception(std::make_unique<NearestFoodPerception>());
    agent.addPerception(std::make_unique<AgentSocialPerception>(18.0f));
    if (config.day_night.enabled) {
        agent.addPerception(std::make_unique<TimeOfDayPerception>());
    }
    if (config.season.enabled) {
        agent.addPerception(std::make_unique<SeasonPerception>());
    }
    // Layer 2 (FINAL_BLUEPRINT.md §2.3): weather perception (6-dim one-hot).
    if (config.weather.enabled) {
        agent.addPerception(std::make_unique<WeatherPerception>());
    }
    if (config.disease.enabled) {
        agent.addPerception(std::make_unique<InfectionPerception>(8.0f));
    }
    if (config.signal.enabled) {
        // Same SignalPerception protagonist uses, sized to match the world's
        // configured channel count. Radius 16 keeps the signal field cheap
        // for background species while still letting an alarm call reach
        // immediate neighbours.
        agent.addPerception(std::make_unique<SignalPerception>(config.signal.channels, /*perception_radius=*/16.0f));
    }
}

// ---------------------------------------------------------------------------
// Species factories
// Each species has a tuned MoveCapability, optional Eat/Attack/Flee/PredatorHunt,
// and species-specific stats (HP / damage / speed) shaped via capability ctor args.
// Output dim per species is computed by summing capability consumedOutputs().
// ---------------------------------------------------------------------------

std::unique_ptr<Agent> makeNeatSpeciesAgent(AgentId id,
                                            SpeciesArchetype archetype,
                                            const NeatGenome& genome,
                                            const ProtagonistEcologyConfig& config,
                                            std::mt19937& rng) {
    float initial_energy = 95.0f;
    bool predator_flag = false;
    switch (archetype) {
        case SpeciesArchetype::LargeHerbivore: initial_energy = 110.0f; break;
        case SpeciesArchetype::SmallForager:   initial_energy = 70.0f;  break;
        case SpeciesArchetype::ApexPredator:   initial_energy = 170.0f; predator_flag = true; break;
        case SpeciesArchetype::PackHunter:     initial_energy = 110.0f; predator_flag = true; break;
        case SpeciesArchetype::AmbushPredator: initial_energy = 90.0f;  predator_flag = true; break;
        case SpeciesArchetype::Scavenger:      initial_energy = 65.0f;  break;
        case SpeciesArchetype::Omnivore:       initial_energy = 100.0f; break;
        case SpeciesArchetype::Rabbit:         initial_energy = 50.0f;  break;  // Layer 3
        default: break;
    }

    auto brain = std::make_unique<NeatBrain>(genome);
    auto agent = std::make_unique<Agent>(
        id,
        genome.id(),
        std::move(brain),
        randomPosition(config.world_size, rng),
        initial_energy,
        20,
        1.0f);
    if (predator_flag) {
        agent->setPredator(true);
    }

    attachBackgroundCommonSensors(*agent, config, initial_energy);

    switch (archetype) {
        case SpeciesArchetype::LargeHerbivore:
            // Large grazers: slow, defensive flee, plant diet.
            // L2.1 v2: damage_threshold=12 means a single PackHunter
            // (4 dmg/sec) accumulates only 4 in a 1s window - below 12,
            // can't kill alone. 3+ wolves simultaneously break the
            // threshold, forcing emergent group hunting.
            agent->setDietPreference(0.05f);  // pure herbivore
            agent->setDamageThreshold(12.0f);
            agent->addCapability(std::make_unique<MoveCapability>(1.8f, 0.0035f, 0.0009f, 20));
            agent->addCapability(std::make_unique<EatCapability>(4.5f, 40.0f));
            agent->addCapability(std::make_unique<FleeCapability>(10.0f, 0.7f, 0.012f));
            // outputs: Move 2 + Eat 0 + Flee 1 = 3
            break;

        case SpeciesArchetype::SmallForager:
            // Small fast rodents: high speed, quick flee, plant diet.
            // L2.1 v2: damage_threshold=0 (fragile, single-bite kill).
            // CaveHideCapability makes them invisible to outside-cave
            // observers when they actually enter a cave terrain cell.
            agent->setDietPreference(0.10f);
            agent->setDamageThreshold(0.0f);
            agent->addCapability(std::make_unique<MoveCapability>(2.3f, 0.005f, 0.0011f, 20));
            agent->addCapability(std::make_unique<EatCapability>(3.5f, 44.0f));
            agent->addCapability(std::make_unique<FleeCapability>(6.0f, 1.0f, 0.012f));
            agent->addCapability(std::make_unique<CaveHideCapability>());
            // outputs: Move 2 + Eat 0 + Flee 1 + CaveHide 0 = 3
            break;

        case SpeciesArchetype::ApexPredator:
            // Tiger/bear: slow but deadly, high single hit.
            // L2.1 v2: damage 18 still breaks LargeHerbivore threshold
            // (12) solo. After landing such a kill, PredatorHuntCapability
            // arms digest_satiety_until = now + 60s and damage scales
            // to 0.2x during that window (apex sleeps it off).
            agent->setDietPreference(0.95f);
            agent->addCapability(std::make_unique<MoveCapability>(1.6f, 0.008f, 0.0014f, 20));
            agent->addCapability(std::make_unique<PredatorHuntCapability>(6.0f, 18.0f, 14.0f, 0.025f));
            agent->addCapability(std::make_unique<AttackCapability>(5.0f, 0.85f, 0.0f, 0.03f));
            // outputs: Move 2 + Hunt 1 + Attack 3 = 6
            break;

        case SpeciesArchetype::PackHunter:
            // Wolves: medium speed, LOW damage (4) so a single wolf
            // can't break LargeHerbivore threshold 12 alone. Pack of
            // 3+ accumulates >= 12 in window and lands a kill burst,
            // forcing emergent group attack.
            agent->setDietPreference(0.90f);
            agent->addCapability(std::make_unique<MoveCapability>(2.0f, 0.006f, 0.0012f, 20));
            agent->addCapability(std::make_unique<PredatorHuntCapability>(5.0f, 4.0f, 5.0f, 0.02f));
            agent->addCapability(std::make_unique<AttackCapability>(4.0f, 0.50f, 0.0f, 0.022f));
            // outputs: Move 2 + Hunt 1 + Attack 3 = 6
            break;

        case SpeciesArchetype::AmbushPredator:
            // Leopard/cat: burst speed, high single damage, low HP.
            // L2.1 v2: StealthCapability flips stealth_state=true after
            // 3s stationary. While stealth, AgentSocialPerception filters
            // self out of distant observers' perception (line-of-sight).
            // First strike out of stealth deals 2x damage, then stealth
            // dissolves (handled in PredatorHuntCapability).
            agent->setDietPreference(0.92f);
            agent->addCapability(std::make_unique<MoveCapability>(2.6f, 0.009f, 0.0015f, 20));
            agent->addCapability(std::make_unique<PredatorHuntCapability>(4.5f, 14.0f, 10.0f, 0.022f));
            agent->addCapability(std::make_unique<AttackCapability>(3.5f, 0.75f, 0.0f, 0.028f));
            agent->addCapability(std::make_unique<StealthCapability>(3.0f, 0.05f));
            // outputs: Move 2 + Hunt 1 + Attack 3 + Stealth 0 = 6
            break;

        case SpeciesArchetype::Scavenger:
            // Vulture/crow: aerial — flies over terrain (no slope penalty).
            // Higher base speed in air, wider eat radius (spotting carrion from above).
            // L2.1 v2: scavenge_efficiency=3.0 means energy gain x3 on
            // low-vitality prey (energy < threshold * 0.25) but only
            // 1/3 on healthy prey - forces "wait for someone else to
            // wound it, then feast" niche even before L2.3 corpse
            // resource channel lands.
            agent->setDietPreference(0.85f);
            agent->setScavengeEfficiency(3.0f);
            agent->addCapability(std::make_unique<MoveCapability>(2.2f, 0.004f, 0.001f, 20, /*aerial=*/true));
            agent->addCapability(std::make_unique<EatCapability>(5.0f, 54.0f));
            agent->addCapability(std::make_unique<PredatorHuntCapability>(4.0f, 7.0f, 5.0f, 0.018f));
            agent->addCapability(std::make_unique<AttackCapability>(3.0f, 0.10f, 0.0f, 0.018f));
            agent->addCapability(std::make_unique<FleeCapability>(8.0f, 0.8f, 0.012f));
            // outputs: Move 2 + Eat 0 + Hunt 1 + Attack 3 + Flee 1 = 7
            // NOTE: backgroundOutputDim must reflect this.
            break;

        case SpeciesArchetype::Omnivore:
            // Boar/bear-cub: flexible diet, medium combat.
            // L2.1 v2: damage_threshold=6 marks omnivore as a middle-
            // toughness prey when itself attacked (between fragile
            // forager and tough herbivore). Single wolf 4dmg < 6 -
            // still needs help to kill an omnivore.
            agent->setDietPreference(0.50f);  // balanced omnivore
            agent->setDamageThreshold(6.0f);
            agent->addCapability(std::make_unique<MoveCapability>(1.7f, 0.005f, 0.0011f, 20));
            agent->addCapability(std::make_unique<EatCapability>(4.0f, 42.0f));
            agent->addCapability(std::make_unique<PredatorHuntCapability>(4.0f, 7.0f, 5.0f, 0.02f));
            agent->addCapability(std::make_unique<AttackCapability>(3.8f, 0.35f, 0.0f, 0.022f));
            agent->addCapability(std::make_unique<FleeCapability>(7.0f, 0.7f, 0.012f));
            // outputs: Move 2 + Eat 0 + Hunt 1 + Attack 3 + Flee 1 = 7
            break;

        case SpeciesArchetype::Rabbit:
            // Layer 3 (FINAL_BLUEPRINT.md D3): stable food-chain bottom.
            // Tiny, fast, fragile (one-bite kill via damage_threshold=0).
            // Pure grass diet; lives in colonies (loneliness setpoint 0
            // applied below). High flee speed (3.0) but no combat.
            agent->setDietPreference(0.05f);  // strict herbivore
            agent->setDamageThreshold(0.0f);  // single-bite kill
            agent->addCapability(std::make_unique<MoveCapability>(2.8f, 0.003f, 0.0008f, 20));  // very fast
            agent->addCapability(std::make_unique<EatCapability>(3.0f, 38.0f));
            agent->addCapability(std::make_unique<FleeCapability>(8.0f, 1.2f, 0.010f));
            // outputs: Move 2 + Eat 0 + Flee 1 = 3
            break;

        default:
            throw std::runtime_error("makeNeatSpeciesAgent: unsupported archetype");
    }

    // L2.2 v2: drive tick + species-specific setpoints. Setpoints
    // table from SPECIES_DIFFERENTIATION_SPEC.md § 2.1. The 5 drives
    // are hunger / thirst / fatigue / fear / loneliness. Default 0
    // means "any deviation is uncomfortable"; a non-zero value means
    // the species "is comfortable being slightly hungry / lonely /
    // afraid". The biggest discriminator is loneliness: pure-pack
    // species (LargeHerbivore, PackHunter) have setpoint 0
    // (must be near peers); apex (ApexPredator, AmbushPredator) have
    // 0.6 (territorial, comfortable alone).
    agent->addCapability(std::make_unique<DriveTickCapability>(/*sense_radius=*/18.0f));
    // L1 (Final Blueprint): thermal regulation R-001 also for background.
    agent->addCapability(std::make_unique<ThermalTickCapability>());
    Agent::DriveSetpoints sp{};
    Agent::EatEfficiencyByKind ee{};
    switch (archetype) {
        case SpeciesArchetype::LargeHerbivore:
            sp.loneliness = 0.0f;
            // Grass specialist: grass perfect, browse mediocre, fruit poor, meat nil.
            ee.grass = 1.0f; ee.browse = 0.5f; ee.fruit = 0.3f; ee.meat = 0.0f;
            break;
        case SpeciesArchetype::SmallForager:
            sp.loneliness = 0.2f;
            // Browse specialist (e.g. nyala): browse perfect, fruit good, grass mediocre.
            ee.grass = 0.5f; ee.browse = 1.0f; ee.fruit = 0.8f; ee.meat = 0.0f;
            break;
        case SpeciesArchetype::ApexPredator:
            sp.loneliness = 0.6f;
            ee.grass = 0.0f; ee.browse = 0.0f; ee.fruit = 0.0f; ee.meat = 1.0f;
            break;
        case SpeciesArchetype::PackHunter:
            sp.loneliness = 0.0f;
            ee.grass = 0.0f; ee.browse = 0.0f; ee.fruit = 0.0f; ee.meat = 1.0f;
            break;
        case SpeciesArchetype::AmbushPredator:
            sp.loneliness = 0.6f;
            ee.grass = 0.0f; ee.browse = 0.0f; ee.fruit = 0.0f; ee.meat = 1.0f;
            break;
        case SpeciesArchetype::Scavenger:
            sp.loneliness = 0.3f;
            // Carrion specialist + a bit of fruit (vultures eat fruit too).
            ee.grass = 0.0f; ee.browse = 0.0f; ee.fruit = 0.5f; ee.meat = 1.0f;
            break;
        case SpeciesArchetype::Omnivore:
            sp.loneliness = 0.0f;
            // Fruit specialist + mid plant + some meat. Matches the
            // boar / bear-cub design (spec: fruit_seasonal Omnivore 1.0).
            ee.grass = 0.6f; ee.browse = 0.6f; ee.fruit = 1.0f; ee.meat = 0.4f;
            break;
        case SpeciesArchetype::Rabbit:
            // Layer 3: colony-living strict herbivore.
            sp.loneliness = 0.0f;   // strongly social
            ee.grass = 1.0f; ee.browse = 0.4f; ee.fruit = 0.3f; ee.meat = 0.0f;
            break;
        default: break;
    }
    agent->setDriveSetpoints(sp);
    agent->setEatEfficiency(ee);

    return agent;
}

std::unique_ptr<Agent> makeBackgroundForager(AgentId id,
                                             GenomeId genome_id,
                                             const ProtagonistEcologyConfig& config,
                                             std::mt19937& rng) {
    constexpr float kInitialEnergy = 95.0f;
    std::size_t sensory_dim = 5;
    auto genome = FixedTopologyGenome::random(
        genome_id,
        sensory_dim,
        config.background.hidden_dim,
        2,
        rng);

    auto brain = std::make_unique<FixedTopologyBrain>(*genome);
    auto agent = std::make_unique<Agent>(
        id,
        genome_id,
        std::move(brain),
        randomPosition(config.world_size, rng),
        kInitialEnergy,
        20,
        1.0f);

    agent->addPerception(std::make_unique<SelfPerception>(kInitialEnergy, 3000.0));
    agent->addPerception(std::make_unique<NearestFoodPerception>());
    agent->addCapability(std::make_unique<MoveCapability>(1.7f, 0.004f, 0.001f, 20));
    agent->addCapability(std::make_unique<EatCapability>(4.0f, 42.0f));
    agent->addCapability(std::make_unique<DriveTickCapability>(/*sense_radius=*/18.0f));
    // L1 (Final Blueprint): thermal regulation R-001 for forager.
    agent->addCapability(std::make_unique<ThermalTickCapability>());
    return agent;
}

std::unique_ptr<Agent> makeBackgroundPredator(AgentId id,
                                              GenomeId genome_id,
                                              const ProtagonistEcologyConfig& config,
                                              std::mt19937& rng) {
    constexpr float kInitialEnergy = 110.0f;
    std::size_t sensory_dim = 5;
    auto genome = FixedTopologyGenome::random(
        genome_id,
        sensory_dim,
        config.background.hidden_dim,
        3,
        rng);

    auto brain = std::make_unique<FixedTopologyBrain>(*genome);
    auto agent = std::make_unique<Agent>(
        id,
        genome_id,
        std::move(brain),
        randomPosition(config.world_size, rng),
        kInitialEnergy,
        20,
        1.0f);

    agent->setPredator(true);
    agent->addPerception(std::make_unique<SelfPerception>(kInitialEnergy, 3000.0));
    agent->addPerception(std::make_unique<NearestFoodPerception>());
    agent->addCapability(std::make_unique<MoveCapability>(2.0f, 0.006f, 0.0012f, 20));
    agent->addCapability(std::make_unique<PredatorHuntCapability>(5.0f, 12.0f, 8.0f, 0.02f));
    agent->addCapability(std::make_unique<DriveTickCapability>(/*sense_radius=*/18.0f));
    // L1 (Final Blueprint): thermal regulation R-001 for predator.
    agent->addCapability(std::make_unique<ThermalTickCapability>());
    return agent;
}

}  // namespace

std::unique_ptr<Agent> makeProtagonistAgent(AgentId id,
                                            const IGenome& genome,
                                            const ProtagonistEcologyConfig& config,
                                            std::mt19937& rng,
                                            const BaseLayer* base_layer) {
    return makeProtagonistAgent(id,
                                genome.id(),
                                dynamic_cast<const ProtagonistGenome*>(&genome),
                                config,
                                base_layer,
                                rng);
}

std::size_t backgroundSensoryDim(const ProtagonistEcologyConfig& config) {
    return computeBackgroundSensoryDim(config);
}

std::size_t backgroundOutputDim(SpeciesArchetype archetype) {
    switch (archetype) {
        case SpeciesArchetype::LargeHerbivore: return 3;  // Move 2 + Flee 1 (CaveHide=0 not on this species)
        case SpeciesArchetype::SmallForager:   return 3;  // Move 2 + Flee 1 (CaveHide consumed=0)
        case SpeciesArchetype::ApexPredator:   return 6;  // Move 2 + Hunt 1 + Attack 3
        case SpeciesArchetype::PackHunter:     return 6;
        case SpeciesArchetype::AmbushPredator: return 6;  // Stealth consumed=0
        // L2.1 v2: Scavenger now has PredatorHuntCapability so it can
        // chip-feed off low-vitality prey via scavenge_efficiency. That
        // adds 1 consumed output (Hunt 1). 7 = Move 2 + Hunt 1 + Attack
        // 3 + Flee 1 (Eat=0).
        case SpeciesArchetype::Scavenger:      return 7;
        case SpeciesArchetype::Omnivore:       return 7;  // Move 2 + Hunt 1 + Attack 3 + Flee 1
        // Layer 3: Rabbit has minimal capability stack - Move 2 + Flee 1.
        case SpeciesArchetype::Rabbit:         return 3;
        default: return 6;
    }
}

std::string_view speciesArchetypeName(SpeciesArchetype archetype) {
    switch (archetype) {
        case SpeciesArchetype::Protagonist:    return "protagonist";
        case SpeciesArchetype::LargeHerbivore: return "large_herbivore";
        case SpeciesArchetype::SmallForager:   return "small_forager";
        case SpeciesArchetype::ApexPredator:   return "apex_predator";
        case SpeciesArchetype::PackHunter:     return "pack_hunter";
        case SpeciesArchetype::AmbushPredator: return "ambush_predator";
        case SpeciesArchetype::Scavenger:      return "scavenger";
        case SpeciesArchetype::Omnivore:       return "omnivore";
        case SpeciesArchetype::Rabbit:         return "rabbit";
    }
    return "unknown";
}

SpeciesArchetype parseSpeciesArchetype(std::string_view name) {
    if (name == "protagonist")     return SpeciesArchetype::Protagonist;
    if (name == "large_herbivore") return SpeciesArchetype::LargeHerbivore;
    if (name == "small_forager")   return SpeciesArchetype::SmallForager;
    if (name == "apex_predator")   return SpeciesArchetype::ApexPredator;
    if (name == "pack_hunter")     return SpeciesArchetype::PackHunter;
    if (name == "ambush_predator") return SpeciesArchetype::AmbushPredator;
    if (name == "scavenger")       return SpeciesArchetype::Scavenger;
    if (name == "omnivore")        return SpeciesArchetype::Omnivore;
    if (name == "rabbit")          return SpeciesArchetype::Rabbit;
    throw std::runtime_error("parseSpeciesArchetype: unknown archetype name '" + std::string(name) + "'");
}

bool isPredatorArchetype(SpeciesArchetype archetype) {
    return archetype == SpeciesArchetype::ApexPredator
        || archetype == SpeciesArchetype::PackHunter
        || archetype == SpeciesArchetype::AmbushPredator;
}

bool isHerbivoreArchetype(SpeciesArchetype archetype) {
    return archetype == SpeciesArchetype::LargeHerbivore
        || archetype == SpeciesArchetype::SmallForager
        || archetype == SpeciesArchetype::Rabbit;  // Layer 3
}

std::unique_ptr<Agent> makeBackgroundAgent(AgentId id,
                                           SpeciesArchetype archetype,
                                           const IGenome& genome,
                                           const ProtagonistEcologyConfig& config,
                                           std::mt19937& rng) {
    const auto* neat = dynamic_cast<const NeatGenome*>(&genome);
    if (neat == nullptr) {
        throw std::runtime_error("makeBackgroundAgent expects a NeatGenome");
    }
    return makeNeatSpeciesAgent(id, archetype, *neat, config, rng);
}

void stampLifecycleAndLineage(Agent& agent,
                              SpeciesArchetype archetype,
                              const ProtagonistEcologyConfig& config,
                              std::uint64_t lineage_id,
                              std::mt19937& jitter_rng) {
    agent.setArchetypeTag(static_cast<std::uint8_t>(archetype));
    agent.setLineageId(lineage_id);
    agent.setParentLineages(0, 0);  // founder
    agent.setBirthGeneration(0);

    // Per-archetype max_age with +-15% jitter for population age spread.
    SimTimeSeconds base_max_age = 0.0;
    switch (archetype) {
        case SpeciesArchetype::Protagonist:    base_max_age = config.population.max_age_protagonist;     break;
        case SpeciesArchetype::LargeHerbivore: base_max_age = config.population.max_age_large_herbivore; break;
        case SpeciesArchetype::SmallForager:   base_max_age = config.population.max_age_small_forager;   break;
        case SpeciesArchetype::ApexPredator:   base_max_age = config.population.max_age_apex_predator;   break;
        case SpeciesArchetype::PackHunter:     base_max_age = config.population.max_age_pack_hunter;     break;
        case SpeciesArchetype::AmbushPredator: base_max_age = config.population.max_age_ambush_predator; break;
        case SpeciesArchetype::Scavenger:      base_max_age = config.population.max_age_scavenger;       break;
        case SpeciesArchetype::Omnivore:       base_max_age = config.population.max_age_omnivore;        break;
        case SpeciesArchetype::Rabbit:         base_max_age = config.population.max_age_rabbit;          break;
    }
    if (base_max_age > 0.0) {
        std::uniform_real_distribution<double> u(0.85, 1.15);
        agent.setMaxAgeSeconds(base_max_age * u(jitter_rng));
    } else {
        agent.setMaxAgeSeconds(0.0);
    }
}

void seedBootstrapPopulation(World2D& world,
                             const ProtagonistEcologyConfig& config,
                             std::uint64_t& next_agent_id,
                             const std::vector<std::unique_ptr<IGenome>>* protagonist_population) {
    auto* agent_layer = world.getLayer<AgentLayer>();
    if (agent_layer == nullptr) {
        throw std::runtime_error("AgentLayer missing from bootstrap world");
    }

    std::mt19937 rng(config.random_seed == 0 ? std::random_device{}() : config.random_seed + 7000);
    std::uint64_t next_genome_id = 1;
    std::uint64_t next_lineage = 1;
    const auto* base_layer = world.getLayer<BaseLayer>();

    if (protagonist_population != nullptr) {
        for (const auto& genome : *protagonist_population) {
            if (genome == nullptr) continue;
            auto agent = makeProtagonistAgent(AgentId{next_agent_id++}, *genome, config, rng, base_layer);
            stampLifecycleAndLineage(*agent, SpeciesArchetype::Protagonist, config, next_lineage++, rng);
            agent_layer->addAgent(std::move(agent), world);
            next_genome_id = std::max(next_genome_id, genome->id().value + 1);
        }
    } else {
        for (std::size_t i = 0; i < config.protagonist.count; ++i) {
            auto agent = makeProtagonistAgent(AgentId{next_agent_id++}, GenomeId{next_genome_id++}, nullptr, config, base_layer, rng);
            stampLifecycleAndLineage(*agent, SpeciesArchetype::Protagonist, config, next_lineage++, rng);
            agent_layer->addAgent(std::move(agent), world);
        }
    }

    for (std::size_t i = 0; i < config.background.forager_count; ++i) {
        auto agent = makeBackgroundForager(AgentId{next_agent_id++}, GenomeId{next_genome_id++}, config, rng);
        stampLifecycleAndLineage(*agent, SpeciesArchetype::SmallForager, config, next_lineage++, rng);
        agent_layer->addAgent(std::move(agent), world);
    }

    for (std::size_t i = 0; i < config.background.predator_count; ++i) {
        auto agent = makeBackgroundPredator(AgentId{next_agent_id++}, GenomeId{next_genome_id++}, config, rng);
        stampLifecycleAndLineage(*agent, SpeciesArchetype::PackHunter, config, next_lineage++, rng);
        agent_layer->addAgent(std::move(agent), world);
    }
}

void seedTrainingPopulation(World2D& world,
                            const ProtagonistEcologyConfig& config,
                            const std::vector<std::unique_ptr<IGenome>>& protagonist_population,
                            const std::vector<BackgroundSpecies>& background_species,
                            std::uint64_t& next_agent_id) {
    auto* agent_layer = world.getLayer<AgentLayer>();
    if (agent_layer == nullptr) {
        throw std::runtime_error("AgentLayer missing from training world");
    }

    std::mt19937 rng(config.random_seed == 0 ? std::random_device{}() : config.random_seed + 7000);
    std::uint64_t next_lineage = 1;
    const auto* base_layer = world.getLayer<BaseLayer>();

    for (const auto& genome : protagonist_population) {
        if (genome == nullptr) {
            continue;
        }
        auto agent = makeProtagonistAgent(AgentId{next_agent_id++}, *genome, config, rng, base_layer);
        stampLifecycleAndLineage(*agent, SpeciesArchetype::Protagonist, config, next_lineage++, rng);
        agent_layer->addAgent(std::move(agent), world);
    }

    for (const auto& species : background_species) {
        if (species.population == nullptr) continue;
        for (const auto& genome : *species.population) {
            if (genome == nullptr) continue;
            auto agent = makeBackgroundAgent(AgentId{next_agent_id++}, species.archetype, *genome, config, rng);
            stampLifecycleAndLineage(*agent, species.archetype, config, next_lineage++, rng);
            // L4: archive genome clone for in-world reproduction.
            agent->setGenomeArchive(genome->clone());
            agent_layer->addAgent(std::move(agent), world);
        }
    }
}

std::unique_ptr<Agent> makeChildFromParents(const Agent& parent_a,
                                            const Agent& parent_b,
                                            AgentId new_agent_id,
                                            GenomeId new_genome_id,
                                            const ProtagonistEcologyConfig& config,
                                            std::mt19937& rng) {
    // Protagonist breeding happens cross-episode through ProtagonistEvolution;
    // skip in-world for now.
    const auto archetype = static_cast<SpeciesArchetype>(parent_a.archetypeTag());
    if (archetype == SpeciesArchetype::Protagonist) return nullptr;

    // Pick the more fit parent's genome as the source for cloning.
    const Agent& src = (parent_a.fitness() >= parent_b.fitness()) ? parent_a : parent_b;
    const IGenome* src_genome = src.genomeArchive();
    if (src_genome == nullptr) return nullptr;

    auto child_genome_base = src_genome->clone();
    auto* child_neat = dynamic_cast<NeatGenome*>(child_genome_base.get());
    if (child_neat == nullptr) return nullptr;
    child_neat->setId(new_genome_id);

    // Apply small Gaussian noise to weights (in-world micro-mutation).
    // Cross-episode evolution still does proper NEAT crossover/mutate.
    std::normal_distribution<float> noise(0.0f, 0.08f);
    auto& conns = child_neat->connections();
    for (auto& c : conns) {
        c.weight += noise(rng);
        if (c.weight > 5.0f) c.weight = 5.0f;
        if (c.weight < -5.0f) c.weight = -5.0f;
    }

    // Build the child Agent using the proper archetype wiring.
    auto child = makeNeatSpeciesAgent(new_agent_id, archetype, *child_neat, config, rng);
    if (child == nullptr) return nullptr;

    // Place the child near the midpoint of its parents.
    const Vec2 pa = parent_a.position();
    const Vec2 pb = parent_b.position();
    std::uniform_real_distribution<float> jitter(-1.5f, 1.5f);
    child->setPosition(Vec2{(pa.x + pb.x) * 0.5f + jitter(rng),
                            (pa.y + pb.y) * 0.5f + jitter(rng)});

    // Copy archetype tag and assign max_age (with jitter for spread).
    stampLifecycleAndLineage(*child, archetype, config, /*lineage=*/0, rng);
    // PopulationLayer will overwrite lineage_id / parents / generation.

    // Archive the (mutated) genome for the child's own future breeding.
    child->setGenomeArchive(std::move(child_genome_base));
    return child;
}

}  // namespace neuro::routes::protagonist
