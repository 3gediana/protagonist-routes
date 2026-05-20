#include "runtime/ProtagonistEcologyRuntime.h"

#include "brain/ProtagonistGenome.h"
#include "core/agent/Agent.h"
#include "core/logging/Logger.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IGenome.h"
#include "core/reward/CombatReward.h"
#include "core/reward/FoodReward.h"
#include "core/reward/IntrinsicReward.h"
#include "core/reward/SurvivalReward.h"
#include "brain/ProtagonistBrain.h"
#include "capability/ChopTreeCapability.h"
#include "capability/CraftSpearCapability.h"
#include "capability/ProtagonistHuntCapability.h"
#include "capability/ThrowCarriedObjectCapability.h"
#include "perception/AgentSocialPerception.h"
#include "perception/CarriedObjectPerception.h"
#include "perception/ObjectLogisticsPerception.h"
#include "perception/StockpilePerception.h"
#include "perception/SurvivalStatePerception.h"
#include "perception/TreePerception.h"
#include "perception/WaterPerception.h"
#include "perception/WeaponPerception.h"
#include "core/brain/neat/NeatBrain.h"
#include "core/brain/neat/NeatGenome.h"
#include "core/evolution/NeatEvolution.h"
#include "core/world/World2D.h"
#include "core/world/layers/AgentLayer.h"
#include "core/world/layers/ResourceLayer.h"
#include "core/world/layers/signal/SignalLayer.h"
#include "runtime/headless/EvaluationPool.h"
#include "runtime/ProtagonistEvolution.h"
#include "runtime/BootstrapPopulation.h"
#include "runtime/ProtagonistEcologyConfig.h"
#include "runtime/ProtagonistWorldFactory.h"
#include "runtime/TraceRecorder.h"
#include "world/CampfireLayer.h"
#include "world/InterventionDirectorLayer.h"
#include "world/NurseryLogisticsLayer.h"
#include "world/WorksiteLayer.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace neuro::routes::protagonist {

namespace {

struct TrainingParams {
    std::size_t generations{0};
    std::size_t episode_ticks{0};
    std::size_t episodes_per_generation{3};
    std::size_t num_threads{0};  // 0 = auto-detect
    double survival_weight{1.0};
    double food_weight{10.0};
    double intrinsic_weight{5.0};
    double combat_weight{1.0};
    double cooperation_weight{6.0};
    double communication_weight{4.0};
    float combat_attack_reward{2.0f};
    float combat_kill_bonus{10.0f};
    float combat_death_penalty{5.0f};
    float worksite_proximity_reward{0.05f};
    float worksite_completion_reward{60.0f};
    float social_proximity_reward{0.01f};
    float social_proximity_radius{6.0f};
    float signal_emit_reward{0.03f};
    float signal_emit_companion_radius{12.0f};
    float signal_co_attendance_reward{0.6f};
    float signal_co_attendance_radius{8.0f};
    std::size_t signal_co_attendance_window{40};
    float joint_hunt_reward{8.0f};
    float joint_hunt_radius{10.0f};
    std::size_t checkpoint_every{0};
    double elite_ratio{0.10};
    std::size_t tournament_size{5};
    double weight_perturb_rate{0.80};
    double weight_reset_rate{0.05};
    double weight_perturb_sigma{0.5};

    double prey_survival_weight{0.6};
    double prey_food_weight{4.0};
    double prey_social_weight{0.012f};
    double prey_retaliation_weight{2.5};
    double prey_death_penalty{1.5};
    double predator_survival_weight{0.4};
    double predator_kill_weight{6.0};
    double predator_attack_weight{1.0};
    double predator_social_weight{0.008f};
    double predator_death_penalty{1.5};
    float prey_social_radius{6.0f};
    float predator_social_radius{8.0f};

    double neat_add_node_rate{0.04};
    double neat_add_connection_rate{0.10};
    double neat_toggle_connection_rate{0.02};
    double neat_add_modulatory_rate{0.005};
    double neat_compatibility_threshold{3.0};
};

struct BehaviorAccumulator {
    std::size_t deposits_total{0};
    std::size_t deposits_stick{0};
    std::size_t deposits_stone{0};
    std::size_t completed_worksites{0};
    std::size_t active_fires{0};
    std::size_t harvested_trees{0};
    std::size_t chop_attempts{0};
    std::size_t chop_successes{0};
    std::size_t craft_attempts{0};
    std::size_t craft_successes{0};
    std::size_t throw_attempts{0};
    std::size_t throw_hits{0};
    std::size_t hunt_attempts{0};
    std::size_t hunt_successes{0};
    std::size_t signal_emits{0};
    std::size_t signal_co_attendance{0};
    std::size_t worksite_completion_events{0};
    std::size_t cooperative_co_presence_ticks{0};
    std::size_t survived_protagonists{0};
    std::size_t total_protagonists{0};
};

struct GenerationStats {
    double min_fitness{0.0};
    double avg_fitness{0.0};
    double best_fitness{0.0};
};

std::string makeTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm{};
#ifdef _WIN32
    localtime_s(&local_tm, &now_time);
#else
    localtime_r(&now_time, &local_tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&local_tm, "%Y%m%d-%H%M%S");
    return oss.str();
}

bool trainingEnabled(const config::Config& config) {
    return config.getOr<std::size_t>("evolution.generations", 0) > 0;
}

TrainingParams loadTrainingParams(const config::Config& config) {
    TrainingParams params;
    params.generations = config.getOr<std::size_t>("evolution.generations", 0);
    params.episode_ticks = config.getOr<std::size_t>("evolution.episode_ticks", 0);
    params.episodes_per_generation = config.getOr<std::size_t>("evolution.episodes_per_generation", params.episodes_per_generation);
    params.num_threads = config.getOr<std::size_t>("evolution.num_threads", params.num_threads);
    params.survival_weight = config.getOr<double>("reward.survival_weight", params.survival_weight);
    params.food_weight = config.getOr<double>("reward.food_weight", params.food_weight);
    params.intrinsic_weight = config.getOr<double>("reward.intrinsic_weight", params.intrinsic_weight);
    params.combat_weight = config.getOr<double>("reward.combat_weight", params.combat_weight);
    params.cooperation_weight = config.getOr<double>("reward.cooperation_weight", params.cooperation_weight);
    params.communication_weight = config.getOr<double>("reward.communication_weight", params.communication_weight);
    params.worksite_proximity_reward = config.getOr<float>("reward.worksite_proximity_reward", params.worksite_proximity_reward);
    params.worksite_completion_reward = config.getOr<float>("reward.worksite_completion_reward", params.worksite_completion_reward);
    params.social_proximity_reward = config.getOr<float>("reward.social_proximity_reward", params.social_proximity_reward);
    params.social_proximity_radius = config.getOr<float>("reward.social_proximity_radius", params.social_proximity_radius);
    params.signal_emit_reward = config.getOr<float>("reward.signal_emit_reward", params.signal_emit_reward);
    params.signal_emit_companion_radius = config.getOr<float>("reward.signal_emit_companion_radius", params.signal_emit_companion_radius);
    params.signal_co_attendance_reward = config.getOr<float>("reward.signal_co_attendance_reward", params.signal_co_attendance_reward);
    params.signal_co_attendance_radius = config.getOr<float>("reward.signal_co_attendance_radius", params.signal_co_attendance_radius);
    params.signal_co_attendance_window = config.getOr<std::size_t>("reward.signal_co_attendance_window", params.signal_co_attendance_window);
    params.joint_hunt_reward = config.getOr<float>("reward.joint_hunt_reward", params.joint_hunt_reward);
    params.joint_hunt_radius = config.getOr<float>("reward.joint_hunt_radius", params.joint_hunt_radius);
    params.checkpoint_every = config.getOr<std::size_t>("evolution.checkpoint_every", params.checkpoint_every);
    params.combat_attack_reward = config.getOr<float>("combat.attack_reward", params.combat_attack_reward);
    params.combat_kill_bonus = config.getOr<float>("combat.kill_bonus", params.combat_kill_bonus);
    params.combat_death_penalty = config.getOr<float>("combat.death_penalty", params.combat_death_penalty);
    params.elite_ratio = config.getOr<double>("evolution.selection.elite_ratio", params.elite_ratio);
    params.tournament_size = config.getOr<std::size_t>("evolution.selection.tournament_size", params.tournament_size);
    params.weight_perturb_rate = config.getOr<double>("evolution.mutation.weight_perturb_rate", params.weight_perturb_rate);
    params.weight_reset_rate = config.getOr<double>("evolution.mutation.weight_reset_rate", params.weight_reset_rate);
    params.weight_perturb_sigma = config.getOr<double>("evolution.mutation.weight_perturb_sigma", params.weight_perturb_sigma);

    params.prey_survival_weight = config.getOr<double>("reward.prey.survival_weight", params.prey_survival_weight);
    params.prey_food_weight = config.getOr<double>("reward.prey.food_weight", params.prey_food_weight);
    params.prey_social_weight = config.getOr<double>("reward.prey.social_weight", params.prey_social_weight);
    params.prey_retaliation_weight = config.getOr<double>("reward.prey.retaliation_weight", params.prey_retaliation_weight);
    params.prey_death_penalty = config.getOr<double>("reward.prey.death_penalty", params.prey_death_penalty);
    params.predator_survival_weight = config.getOr<double>("reward.predator.survival_weight", params.predator_survival_weight);
    params.predator_kill_weight = config.getOr<double>("reward.predator.kill_weight", params.predator_kill_weight);
    params.predator_attack_weight = config.getOr<double>("reward.predator.attack_weight", params.predator_attack_weight);
    params.predator_social_weight = config.getOr<double>("reward.predator.social_weight", params.predator_social_weight);
    params.predator_death_penalty = config.getOr<double>("reward.predator.death_penalty", params.predator_death_penalty);
    params.prey_social_radius = config.getOr<float>("reward.prey.social_radius", params.prey_social_radius);
    params.predator_social_radius = config.getOr<float>("reward.predator.social_radius", params.predator_social_radius);

    params.neat_add_node_rate = config.getOr<double>("evolution.neat.add_node_rate", params.neat_add_node_rate);
    params.neat_add_connection_rate = config.getOr<double>("evolution.neat.add_connection_rate", params.neat_add_connection_rate);
    params.neat_toggle_connection_rate = config.getOr<double>("evolution.neat.toggle_connection_rate", params.neat_toggle_connection_rate);
    params.neat_add_modulatory_rate = config.getOr<double>("evolution.neat.add_modulatory_rate", params.neat_add_modulatory_rate);
    params.neat_compatibility_threshold = config.getOr<double>("evolution.neat.compatibility_threshold", params.neat_compatibility_threshold);

    return params;
}

std::filesystem::path makeRunDir(const config::Config& config) {
    const auto runs_dir = std::filesystem::path(config.getOr<std::string>("output.runs_dir", "data/runs/protagonist"));
    const auto run_dir = runs_dir / makeTimestamp();
    std::filesystem::create_directories(run_dir);
    return run_dir;
}

std::string jsonEscape(std::string_view input) {
    std::string out;
    out.reserve(input.size() + 8);
    for (const char ch : input) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += ch; break;
        }
    }
    return out;
}

std::vector<std::unique_ptr<IGenome>> createInitialPopulation(const ProtagonistEcologyConfig& route) {
    std::mt19937 rng(route.random_seed == 0 ? std::random_device{}() : route.random_seed);
    std::vector<std::unique_ptr<IGenome>> population;
    population.reserve(route.protagonist.count);
    const auto sensory_dim = protagonistSensoryDim(route);
    for (std::size_t i = 0; i < route.protagonist.count; ++i) {
        population.push_back(ProtagonistGenome::random(
            GenomeId{i + 1},
            sensory_dim,
            route.protagonist.hidden_dim,
            route.protagonist.memory_cells,
            route.protagonist.action_dim,
            rng));
    }
    return population;
}

std::vector<std::unique_ptr<IGenome>> loadProtagonistCheckpointPopulation(const ProtagonistEcologyConfig& route,
                                                                         const std::filesystem::path& checkpoint_dir) {
    if (!std::filesystem::exists(checkpoint_dir) || !std::filesystem::is_directory(checkpoint_dir)) {
        throw std::runtime_error("runtime.resume_checkpoint_path is not a directory: " + checkpoint_dir.string());
    }

    std::vector<std::filesystem::path> genome_paths;
    for (const auto& entry : std::filesystem::directory_iterator(checkpoint_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto filename = entry.path().filename().string();
        if (filename.starts_with("genome_") && entry.path().extension() == ".bin") {
            genome_paths.push_back(entry.path());
        }
    }
    std::sort(genome_paths.begin(), genome_paths.end());

    std::vector<std::unique_ptr<IGenome>> population;
    population.reserve(genome_paths.size());
    for (const auto& path : genome_paths) {
        auto genome = ProtagonistGenome::loadFromFile(path);
        if (genome == nullptr) {
            throw std::runtime_error("failed to load protagonist checkpoint genome: " + path.string());
        }
        if (genome->inputDim() != protagonistSensoryDim(route)
            || genome->hiddenDim() != route.protagonist.hidden_dim
            || genome->memoryCells() != route.protagonist.memory_cells
            || genome->actionDim() != route.protagonist.action_dim) {
            throw std::runtime_error("checkpoint protagonist genome shape does not match current scenario: " + path.string());
        }
        population.push_back(std::move(genome));
    }

    if (population.empty()) {
        throw std::runtime_error("runtime.resume_checkpoint_path contains no protagonist genome_*.bin files: " + checkpoint_dir.string());
    }
    return population;
}

std::vector<std::unique_ptr<IGenome>> createTrainingPopulationFromConfig(const config::Config& config,
                                                                         const ProtagonistEcologyConfig& route) {
    const auto checkpoint_path = config.getOr<std::string>("runtime.resume_checkpoint_path", "");
    if (checkpoint_path.empty()) {
        return createInitialPopulation(route);
    }

    auto population = loadProtagonistCheckpointPopulation(route, std::filesystem::path(checkpoint_path));
    LOG_INFO("ProtagonistEcologyRuntime: resumed protagonist population count={} from {}",
             population.size(),
             checkpoint_path);
    return population;
}

std::vector<std::unique_ptr<IGenome>> createPopulationFromGenomeFile(const ProtagonistEcologyConfig& route,
                                                                     const std::filesystem::path& genome_path) {
    auto loaded = ProtagonistGenome::loadFromFile(genome_path);
    if (loaded == nullptr) {
        throw std::runtime_error("failed to load protagonist genome: " + genome_path.string());
    }

    if (loaded->inputDim() != protagonistSensoryDim(route)
        || loaded->hiddenDim() != route.protagonist.hidden_dim
        || loaded->memoryCells() != route.protagonist.memory_cells
        || loaded->actionDim() != route.protagonist.action_dim) {
        throw std::runtime_error("loaded protagonist genome shape does not match current scenario");
    }

    std::vector<std::unique_ptr<IGenome>> population;
    population.reserve(route.protagonist.count);
    for (std::size_t i = 0; i < route.protagonist.count; ++i) {
        auto cloned = std::make_unique<ProtagonistGenome>(*loaded);
        cloned->setId(GenomeId{i + 1});
        population.push_back(std::move(cloned));
    }
    return population;
}

std::vector<std::unique_ptr<IGenome>> createConfiguredPreviewPopulation(const config::Config& config,
                                                                        const ProtagonistEcologyConfig& route) {
    const auto genome_path = config.getOr<std::string>("runtime.protagonist_genome_path", "");
    if (genome_path.empty()) {
        return {};
    }
    return createPopulationFromGenomeFile(route, std::filesystem::path(genome_path));
}

std::vector<IGenome*> rawGenomePointers(std::vector<std::unique_ptr<IGenome>>& population) {
    std::vector<IGenome*> raw;
    raw.reserve(population.size());
    for (auto& genome : population) {
        raw.push_back(genome.get());
    }
    return raw;
}

std::vector<std::unique_ptr<IGenome>> createInitialNeatPopulation(std::size_t count,
                                                                  std::size_t input_dim,
                                                                  std::size_t output_dim,
                                                                  GenomeId start_id,
                                                                  InnovationTracker& tracker,
                                                                  std::mt19937& rng) {
    std::vector<std::unique_ptr<IGenome>> population;
    population.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        population.push_back(NeatGenome::minimal(
            GenomeId{start_id.value + i},
            input_dim,
            output_dim,
            tracker,
            rng));
    }
    return population;
}

double evaluateFoodReward(const Agent& agent, const IWorld& world) {
    (void)world;
    const double plant = static_cast<double>(agent.plantFoodThisTick());
    const double meat = static_cast<double>(agent.meatFoodThisTick());
    const double total = plant + meat;
    const double diet = static_cast<double>(agent.dietPreference());
    const double match = total > 0.0
        ? (((1.0 - diet) * plant) + (diet * meat)) / total
        : 0.0;
    return total > 0.0 ? total * (0.5 + 0.75 * match) : static_cast<double>(agent.foodEatenThisTick());
}

double evaluateIntrinsicReward(const Agent& agent) {
    if (!agent.discoveredNewCellThisTick()) {
        return 0.0;
    }
    const double visited_cells = static_cast<double>(std::max<std::size_t>(std::size_t{1}, agent.visitedCellCount()));
    return 1.0 / std::sqrt(visited_cells);
}

void evaluatePopulation(World2D& world,
                        const TrainingParams& params,
                        const std::vector<std::unique_ptr<IGenome>>& population,
                        const std::vector<BackgroundSpecies>& background_species,
                        BehaviorAccumulator& behavior,
                        const std::atomic<bool>& stop_requested,
                        TraceRecorder* trace_recorder) {
    auto* agent_layer = world.getLayer<AgentLayer>();
    if (agent_layer == nullptr) {
        throw std::runtime_error("AgentLayer missing from protagonist training world");
    }
    auto* worksite_layer = world.getLayer<WorksiteLayer>();
    auto* logistics_layer = world.getLayer<NurseryLogisticsLayer>();
    auto* campfire_layer = world.getLayer<CampfireLayer>();
    auto* signal_layer = world.getLayer<SignalLayer>();
    auto* resource_layer = world.getLayer<ResourceLayer>();

    std::unordered_set<GenomeId> protagonist_ids;
    protagonist_ids.reserve(population.size());
    for (const auto& genome : population) {
        protagonist_ids.insert(genome->id());
    }
    std::unordered_map<GenomeId, SpeciesArchetype> bg_archetype_by_id;
    for (const auto& species : background_species) {
        if (species.population == nullptr) continue;
        for (const auto& genome : *species.population) {
            if (genome != nullptr) {
                bg_archetype_by_id.emplace(genome->id(), species.archetype);
            }
        }
    }

    const std::size_t worksite_completion_baseline = worksite_layer != nullptr ? worksite_layer->completedSiteCount() : 0;
    std::size_t worksite_completion_running = worksite_completion_baseline;
    struct SignalEmitInfo {
        std::size_t tick{0};
        Vec2 position{};
    };
    std::unordered_map<AgentId, SignalEmitInfo> last_signal_emit;
    std::vector<events::AgentEmittedSignal> tick_emits;
    tick_emits.reserve(64);
    std::unordered_map<AgentId, float> previous_damage;
    std::unordered_map<AgentId, std::size_t> previous_kills;
    std::unordered_set<AgentId> alive_last_tick;

    std::unordered_map<AgentId, float> bg_previous_damage;
    std::unordered_map<AgentId, std::size_t> bg_previous_kills;
    std::unordered_set<AgentId> bg_alive_last_tick;

    auto subscribe_signals = signal_layer != nullptr ? world.eventBus().subscribe(
        events::EventType::AgentEmittedSignal,
        [&](const events::Event& event) {
            const auto& payload = event.payloadAs<events::AgentEmittedSignal>();
            last_signal_emit[payload.id] = SignalEmitInfo{event.tick(), payload.pos};
            tick_emits.push_back(payload);
        }) : 0u;

    // Spawn corpse meat at death position when any agent dies.
    // Scavengers/predators/omnivores can then eat it via EatCapability.
    // Corpse energy = 60% of an average forager's initial energy.
    constexpr float kCorpseEnergyDefault = 60.0f;
    auto subscribe_died = (resource_layer != nullptr) ? world.eventBus().subscribe(
        events::EventType::AgentDied,
        [agent_layer, resource_layer, &world](const events::Event& event) {
            const auto& payload = event.payloadAs<events::AgentDied>();
            for (auto* a : agent_layer->agents()) {
                if (a != nullptr && a->id() == payload.id) {
                    resource_layer->addCorpse(a->position(), kCorpseEnergyDefault, world);
                    break;
                }
            }
        }) : 0u;

    for (std::size_t tick = 0; tick < params.episode_ticks; ++tick) {
        if (stop_requested.load()) {
            break;
        }

        tick_emits.clear();
        world.tick(1.0f);
        bool any_alive = false;

        std::size_t completed_this_tick = 0;
        if (worksite_layer != nullptr) {
            const auto current_completed = worksite_layer->completedSiteCount();
            if (current_completed > worksite_completion_running) {
                completed_this_tick = current_completed - worksite_completion_running;
                worksite_completion_running = current_completed;
                behavior.worksite_completion_events += completed_this_tick;
            }
        }

        std::vector<Agent*> alive_protagonists;
        alive_protagonists.reserve(population.size());
        for (auto* agent : agent_layer->agents()) {
            if (agent == nullptr || !protagonist_ids.contains(agent->genomeId())) {
                continue;
            }
            auto* concrete = dynamic_cast<Agent*>(agent);
            if (concrete != nullptr && concrete->alive()) {
                alive_protagonists.push_back(concrete);
            }
        }

        for (auto* agent : agent_layer->agents()) {
            if (agent == nullptr || !protagonist_ids.contains(agent->genomeId())) {
                continue;
            }
            auto* concrete = dynamic_cast<Agent*>(agent);
            if (concrete == nullptr) {
                continue;
            }

            double reward = 0.0;
            const bool alive_now = concrete->alive();
            if (alive_now) {
                any_alive = true;
                reward += params.survival_weight * 0.01;
                reward += params.food_weight * evaluateFoodReward(*concrete, world);
                reward += params.intrinsic_weight * evaluateIntrinsicReward(*concrete);
                alive_last_tick.insert(agent->id());
            } else if (alive_last_tick.contains(agent->id())) {
                reward -= params.combat_weight * static_cast<double>(params.combat_death_penalty);
                alive_last_tick.erase(agent->id());
            }

            const float current_damage = concrete->damageDealt();
            const auto current_kills = concrete->kills();
            const float damage_delta = current_damage - previous_damage[agent->id()];
            const std::size_t previous_kill_count = previous_kills[agent->id()];
            const std::size_t kill_delta = current_kills >= previous_kill_count
                ? current_kills - previous_kill_count
                : 0;
            previous_damage[agent->id()] = current_damage;
            previous_kills[agent->id()] = current_kills;
            reward += params.combat_weight * static_cast<double>(std::max(0.0f, damage_delta) * params.combat_attack_reward);
            reward += params.combat_weight * static_cast<double>(kill_delta) * params.combat_kill_bonus;

            if (alive_now) {
                bool inside_active_worksite = false;
                if (worksite_layer != nullptr) {
                    for (const auto& site : worksite_layer->sites()) {
                        if (!site.active || site.completed) {
                            continue;
                        }
                        if (Vec2::distanceSquared(concrete->position(), site.position) <= site.radius * site.radius) {
                            inside_active_worksite = true;
                            break;
                        }
                    }
                }
                if (inside_active_worksite) {
                    reward += params.cooperation_weight * static_cast<double>(params.worksite_proximity_reward);
                }

                if (completed_this_tick > 0 && inside_active_worksite) {
                    reward += params.cooperation_weight * static_cast<double>(params.worksite_completion_reward);
                }

                std::size_t neighbours = 0;
                for (const auto* peer : alive_protagonists) {
                    if (peer == nullptr || peer == concrete) {
                        continue;
                    }
                    if (Vec2::distanceSquared(concrete->position(), peer->position()) <= params.social_proximity_radius * params.social_proximity_radius) {
                        ++neighbours;
                        if (neighbours >= 2) break;
                    }
                }
                if (neighbours >= 1) {
                    reward += params.cooperation_weight * static_cast<double>(params.social_proximity_reward) * static_cast<double>(neighbours);
                    if (neighbours >= 2) {
                        ++behavior.cooperative_co_presence_ticks;
                    }
                }

                if (signal_layer != nullptr) {
                    bool emitted_now = false;
                    Vec2 emit_pos{};
                    for (const auto& payload : tick_emits) {
                        if (payload.id == agent->id()) {
                            emitted_now = true;
                            emit_pos = payload.pos;
                            break;
                        }
                    }
                    if (emitted_now) {
                        for (const auto* peer : alive_protagonists) {
                            if (peer == nullptr || peer == concrete) {
                                continue;
                            }
                            if (Vec2::distanceSquared(emit_pos, peer->position()) <= params.signal_emit_companion_radius * params.signal_emit_companion_radius) {
                                reward += params.communication_weight * static_cast<double>(params.signal_emit_reward);
                                break;
                            }
                        }
                    }

                    const auto signal_emit_it = last_signal_emit.find(agent->id());
                    if (signal_emit_it != last_signal_emit.end()) {
                        const std::size_t emitted_at = signal_emit_it->second.tick;
                        const std::size_t now_tick = world.currentTick();
                        if (now_tick > emitted_at && now_tick - emitted_at <= params.signal_co_attendance_window) {
                            for (const auto* peer : alive_protagonists) {
                                if (peer == nullptr || peer == concrete) {
                                    continue;
                                }
                                if (Vec2::distanceSquared(concrete->position(), peer->position()) <= params.signal_co_attendance_radius * params.signal_co_attendance_radius
                                    && peer->foodEatenThisTick() > 0.0f) {
                                    reward += params.communication_weight * static_cast<double>(params.signal_co_attendance_reward);
                                    ++behavior.signal_co_attendance;
                                    break;
                                }
                            }
                        }
                    }
                }

                if (kill_delta > 0) {
                    for (const auto* peer : alive_protagonists) {
                        if (peer == nullptr || peer == concrete) {
                            continue;
                        }
                        if (Vec2::distanceSquared(concrete->position(), peer->position()) <= params.joint_hunt_radius * params.joint_hunt_radius) {
                            reward += params.cooperation_weight * static_cast<double>(params.joint_hunt_reward);
                            break;
                        }
                    }
                }
            }

            if (reward != 0.0) {
                agent->addFitness(reward);
            }
        }

        for (auto* agent : agent_layer->agents()) {
            if (agent == nullptr) continue;
            const auto archetype_it = bg_archetype_by_id.find(agent->genomeId());
            if (archetype_it == bg_archetype_by_id.end()) continue;
            const SpeciesArchetype archetype = archetype_it->second;
            auto* concrete = dynamic_cast<Agent*>(agent);
            if (concrete == nullptr) continue;

            const bool is_predator = isPredatorArchetype(archetype);
            const bool is_herbivore = isHerbivoreArchetype(archetype);
            const bool is_scavenger = (archetype == SpeciesArchetype::Scavenger);
            const bool is_omnivore = (archetype == SpeciesArchetype::Omnivore);

            double bg_reward = 0.0;
            const bool alive_now = concrete->alive();
            if (alive_now) {
                bg_alive_last_tick.insert(agent->id());

                // Per-archetype survival + base food
                if (is_herbivore) {
                    bg_reward += params.prey_survival_weight * 0.005;
                    bg_reward += params.prey_food_weight * static_cast<double>(concrete->foodEatenThisTick());
                    if (archetype == SpeciesArchetype::SmallForager) {
                        // Small foragers are valued more on plant intake (scaled down survival)
                        bg_reward += params.prey_food_weight * 0.5 * static_cast<double>(concrete->plantFoodThisTick());
                    }
                } else if (is_predator) {
                    bg_reward += params.predator_survival_weight * 0.005;
                } else if (is_scavenger) {
                    bg_reward += params.prey_survival_weight * 0.004;
                    bg_reward += params.prey_food_weight * 0.6 * static_cast<double>(concrete->foodEatenThisTick());
                    bg_reward += params.predator_kill_weight * 0.05 * static_cast<double>(concrete->meatFoodThisTick());
                } else if (is_omnivore) {
                    bg_reward += params.prey_survival_weight * 0.005;
                    bg_reward += params.prey_food_weight * 0.7 * static_cast<double>(concrete->plantFoodThisTick());
                    bg_reward += params.predator_kill_weight * 0.3 * static_cast<double>(concrete->meatFoodThisTick());
                }

                const float current_damage = concrete->damageDealt();
                const auto current_kills = concrete->kills();
                const float damage_delta = current_damage - bg_previous_damage[agent->id()];
                const std::size_t prev_kill_count = bg_previous_kills[agent->id()];
                const std::size_t kill_delta = current_kills > prev_kill_count ? current_kills - prev_kill_count : 0;
                bg_previous_damage[agent->id()] = current_damage;
                bg_previous_kills[agent->id()] = current_kills;

                if (damage_delta > 0.0f) {
                    if (is_herbivore) {
                        bg_reward += params.prey_retaliation_weight * static_cast<double>(damage_delta);
                    } else if (is_predator) {
                        bg_reward += params.predator_attack_weight * static_cast<double>(damage_delta);
                    } else if (is_scavenger) {
                        bg_reward += params.prey_retaliation_weight * 0.3 * static_cast<double>(damage_delta);
                    } else if (is_omnivore) {
                        bg_reward += params.predator_attack_weight * 0.5 * static_cast<double>(damage_delta);
                    }
                }
                if (is_predator) {
                    bg_reward += params.predator_kill_weight * static_cast<double>(kill_delta);
                    // PackHunter: bonus when killing near same-species peers
                    if (archetype == SpeciesArchetype::PackHunter && kill_delta > 0) {
                        std::size_t pack_buddies = 0;
                        const float pack_radius_sq = params.predator_social_radius * params.predator_social_radius;
                        for (auto* peer : agent_layer->agents()) {
                            if (peer == nullptr || peer == agent) continue;
                            if (!peer->alive()) continue;
                            const auto peer_it = bg_archetype_by_id.find(peer->genomeId());
                            if (peer_it == bg_archetype_by_id.end()
                                || peer_it->second != SpeciesArchetype::PackHunter) continue;
                            if (Vec2::distanceSquared(concrete->position(), peer->position()) <= pack_radius_sq) {
                                ++pack_buddies;
                                if (pack_buddies >= 3) break;
                            }
                        }
                        if (pack_buddies > 0) {
                            bg_reward += params.predator_kill_weight * 0.5 * static_cast<double>(pack_buddies);
                        }
                    }
                } else if (is_omnivore) {
                    bg_reward += params.predator_kill_weight * 0.4 * static_cast<double>(kill_delta);
                }

                // Same-species social bonus (small)
                const float social_radius = is_predator
                    ? params.predator_social_radius
                    : params.prey_social_radius;
                const float social_radius_sq = social_radius * social_radius;
                std::size_t same_neighbours = 0;
                for (auto* peer : agent_layer->agents()) {
                    if (peer == nullptr || peer == agent) continue;
                    if (!peer->alive()) continue;
                    const auto peer_it = bg_archetype_by_id.find(peer->genomeId());
                    if (peer_it == bg_archetype_by_id.end() || peer_it->second != archetype) continue;
                    if (Vec2::distanceSquared(concrete->position(), peer->position()) <= social_radius_sq) {
                        ++same_neighbours;
                        if (same_neighbours >= 4) break;
                    }
                }
                if (same_neighbours > 0) {
                    const double social_w = is_predator ? params.predator_social_weight : params.prey_social_weight;
                    bg_reward += social_w * static_cast<double>(same_neighbours);
                }
            } else if (bg_alive_last_tick.contains(agent->id())) {
                bg_reward -= is_predator ? params.predator_death_penalty : params.prey_death_penalty;
                bg_alive_last_tick.erase(agent->id());
            }

            if (bg_reward != 0.0) {
                concrete->addFitness(bg_reward);
            }
        }

        if (trace_recorder != nullptr) {
            trace_recorder->recordTick(world);
        }

        if (!any_alive) {
            break;
        }
    }

    if (signal_layer != nullptr && subscribe_signals != 0) {
        world.eventBus().unsubscribe(subscribe_signals);
    }
    if (resource_layer != nullptr && subscribe_died != 0) {
        world.eventBus().unsubscribe(subscribe_died);
    }

    behavior.signal_emits += last_signal_emit.size();
    if (logistics_layer != nullptr) {
        behavior.deposits_total += logistics_layer->totalDeposits();
        behavior.deposits_stick += logistics_layer->totalDeposits(ObjectType::Stick);
        behavior.deposits_stone += logistics_layer->totalDeposits(ObjectType::Stone);
    }
    if (worksite_layer != nullptr) {
        behavior.completed_worksites += worksite_completion_running - worksite_completion_baseline;
    }
    if (campfire_layer != nullptr) {
        behavior.active_fires += campfire_layer->activeFireCount();
    }
    behavior.chop_attempts += ChopTreeCapability::chop_attempts.load();
    behavior.chop_successes += ChopTreeCapability::chop_successes.load();
    behavior.craft_attempts += CraftSpearCapability::craft_attempts.load();
    behavior.craft_successes += CraftSpearCapability::craft_successes.load();
    behavior.throw_attempts += ThrowCarriedObjectCapability::throw_attempts.load();
    behavior.throw_hits += ThrowCarriedObjectCapability::throw_hits.load();
    behavior.hunt_attempts += ProtagonistHuntCapability::hunt_attempts.load();
    behavior.hunt_successes += ProtagonistHuntCapability::hunt_successes.load();
    for (const auto* agent : agent_layer->agents()) {
        if (agent == nullptr) continue;
        if (!protagonist_ids.contains(agent->genomeId())) continue;
        ++behavior.total_protagonists;
        if (agent->alive()) {
            ++behavior.survived_protagonists;
        }
    }
}

void accumulateGenomeFitness(World2D& world,
                             const std::vector<std::unique_ptr<IGenome>>& population,
                             std::unordered_map<GenomeId, double>& fitness_sum) {
    auto* agent_layer = world.getLayer<AgentLayer>();
    if (agent_layer == nullptr) {
        return;
    }
    std::unordered_set<GenomeId> protagonist_ids;
    protagonist_ids.reserve(population.size());
    for (const auto& genome : population) {
        if (genome != nullptr) {
            protagonist_ids.insert(genome->id());
        }
    }
    for (const auto* agent : agent_layer->agents()) {
        if (agent == nullptr || !protagonist_ids.contains(agent->genomeId())) {
            continue;
        }
        fitness_sum[agent->genomeId()] += agent->fitness();
    }
}

std::vector<EvaluatedIndividual> collectEvaluated(const std::vector<std::unique_ptr<IGenome>>& population,
                                                  const std::unordered_map<GenomeId, double>& fitness_sum,
                                                  std::size_t episodes) {
    std::vector<EvaluatedIndividual> evaluated;
    evaluated.reserve(population.size());
    const double divisor = episodes == 0 ? 1.0 : static_cast<double>(episodes);
    for (const auto& genome : population) {
        if (genome == nullptr) {
            continue;
        }
        const auto it = fitness_sum.find(genome->id());
        const double fitness = it == fitness_sum.end() ? 0.0 : it->second / divisor;
        evaluated.push_back(EvaluatedIndividual{
            genome->id(),
            fitness,
            kInvalidSpeciesId,
            genome->complexity(),
            0,
            0.05f,
            {},
        });
    }
    return evaluated;
}

GenerationStats computeStats(const std::vector<EvaluatedIndividual>& evaluated) {
    if (evaluated.empty()) {
        return {};
    }

    GenerationStats stats{};
    stats.min_fitness = evaluated.front().fitness;
    stats.best_fitness = evaluated.front().fitness;
    double total = 0.0;
    for (const auto& item : evaluated) {
        stats.min_fitness = std::min(stats.min_fitness, item.fitness);
        stats.best_fitness = std::max(stats.best_fitness, item.fitness);
        total += item.fitness;
    }
    stats.avg_fitness = total / static_cast<double>(evaluated.size());
    return stats;
}

}  // namespace

ProtagonistEcologyRuntime::ProtagonistEcologyRuntime(config::Config config)
    : config_(std::move(config)) {}

ProtagonistEcologyRuntime::~ProtagonistEcologyRuntime() {
    if (running_.load()) {
        stop();
    }
}

void ProtagonistEcologyRuntime::initialize() {
    if (initialized_) {
        return;
    }

    const auto route = ProtagonistEcologyConfig::fromConfig(config_);
    ChopTreeCapability::resetCounters();
    CraftSpearCapability::resetCounters();
    ProtagonistHuntCapability::resetCounters();
    ThrowCarriedObjectCapability::resetCounters();
    world_ = createBootstrapWorld(route);
    auto preview_population = createConfiguredPreviewPopulation(config_, route);
    std::uint64_t next_agent_id = 1;
    seedBootstrapPopulation(*world_, route, next_agent_id, preview_population.empty() ? nullptr : &preview_population);
    if (!preview_population.empty()) {
        LOG_INFO("ProtagonistEcologyRuntime: loaded protagonist genome population for preview/eval count={}", preview_population.size());
    }

    LOG_INFO(
        "ProtagonistEcologyRuntime: world ready (role={}, scenario={}, mode_train={}, mode_eval={}, mode_live={}, world={}x{}, protagonist={}, background_foragers={}, background_predators={}, protagonist_hidden={}, protagonist_memory={})",
        route.world_role,
        route.scenario_name,
        route.isTrainRole(),
        route.isEvalRole(),
        route.isLiveRole(),
        route.world_size.x,
        route.world_size.y,
        route.protagonist.count,
        route.background.forager_count,
        route.background.predator_count,
        route.protagonist.hidden_dim,
        route.protagonist.memory_cells);
    const bool has_tree_curriculum = route.tree.enabled && route.objects.enabled && route.base.enabled;
    const bool has_tool_curriculum = route.objects.enabled && route.base.enabled;
    const ProtagonistBrain brain_probe(
        2 + 3 + 10 + (route.water.enabled ? WaterPerception::kDim : 0) + (route.survival.enabled ? SurvivalStatePerception::kDim : 0) + ((route.background.forager_count > 0 || route.background.predator_count > 0) ? AgentSocialPerception::kDim : 0) + (has_tree_curriculum ? TreePerception::kDim : 0) + (has_tool_curriculum ? (CarriedObjectPerception::kDim + StockpilePerception::kDim + WeaponPerception::kDim) : 0) + (route.objects.enabled ? ObjectLogisticsPerception::kDim : 0) + (route.campfire.enabled ? 5 : 0) + (route.worksite.enabled ? 8 : 0) + (route.terrain_enabled ? 5 : 0) + (route.signal.enabled ? route.signal.channels * 2 : 0),
        route.protagonist.hidden_dim,
        route.protagonist.memory_cells,
        route.protagonist.action_dim,
        route.random_seed);
    LOG_INFO("ProtagonistEcologyRuntime: protagonist brain parameters per agent={}", brain_probe.parameterCount());

    if (const auto* agent_layer = world_->getLayer<AgentLayer>(); agent_layer != nullptr) {
        LOG_INFO("ProtagonistEcologyRuntime: attached layers ready, current_agents={}, living_agents={}", agent_layer->size(), agent_layer->livingCount());
    }

    initialized_ = true;
}

int ProtagonistEcologyRuntime::run() {
    running_.store(true);
    stop_requested_.store(false);

    if (trainingEnabled(config_)) {
        const int rc = runTraining();
        running_.store(false);
        return rc;
    }

    if (!initialized_) {
        initialize();
    }

    const auto route = ProtagonistEcologyConfig::fromConfig(config_);
    for (std::size_t tick = 0; tick < route.preview_ticks && !stop_requested_.load(); ++tick) {
        world_->tick(1.0f);
    }

    LOG_INFO("ProtagonistEcologyRuntime: scenario {} ({}) ticked {} step(s)", route.scenario_name, route.world_role, route.preview_ticks);

    running_.store(false);
    return 0;
}

int ProtagonistEcologyRuntime::runTraining() {
    const auto route = ProtagonistEcologyConfig::fromConfig(config_);
    const auto params = loadTrainingParams(config_);
    if (params.generations == 0 || params.episode_ticks == 0) {
        LOG_ERROR("ProtagonistEcologyRuntime: invalid training params generations={} episode_ticks={}", params.generations, params.episode_ticks);
        return 1;
    }
    const std::size_t episodes_per_generation = std::max<std::size_t>(params.episodes_per_generation, 1);

    // Multi-threaded episode evaluation. We cap thread count at episodes_per_generation
    // because each episode runs as a single task; more threads than tasks gains nothing.
    const std::size_t requested_threads = params.num_threads;
    const std::size_t hw_threads = std::max<std::size_t>(1u, std::thread::hardware_concurrency());
    const std::size_t auto_threads = hw_threads > 1 ? hw_threads - 1 : 1;
    const std::size_t pool_size = std::min(
        episodes_per_generation,
        requested_threads == 0 ? auto_threads : requested_threads);
    runtime::EvaluationPool evaluation_pool(pool_size);

    const auto run_dir = makeRunDir(config_);
    LOG_INFO("ProtagonistEcologyRuntime: training initialize (config={}, run_dir={}, episodes_per_generation={}, pool_threads={})",
             config_.path().string(),
             run_dir.string(),
             episodes_per_generation,
             pool_size);

    const auto trace_config = loadTraceRecorderConfig(config_);
    auto population = createTrainingPopulationFromConfig(config_, route);
    ProtagonistEvolution evolution(
        params.elite_ratio,
        params.tournament_size,
        params.weight_perturb_rate,
        params.weight_reset_rate,
        params.weight_perturb_sigma,
        route.random_seed + 5000);

    auto make_neat_evolution = [&](std::uint32_t seed_offset) {
        return std::make_unique<NeatEvolution>(
            params.elite_ratio,
            params.tournament_size,
            params.weight_perturb_rate,
            params.weight_reset_rate,
            params.neat_add_node_rate,
            params.neat_add_connection_rate,
            params.neat_toggle_connection_rate,
            params.neat_add_modulatory_rate,
            params.weight_perturb_sigma,
            params.neat_compatibility_threshold,
            1.0, 1.0, 0.4,
            0.1f, 5.0f,
            route.random_seed + seed_offset);
    };

    // ---------------- species slot resolution ----------------
    // Resolve config slots into an ordered list with archetype + count.
    struct ResolvedSlot {
        SpeciesArchetype archetype;
        std::size_t count;
    };
    std::vector<ResolvedSlot> resolved_slots;
    if (!route.background.slots.empty()) {
        resolved_slots.reserve(route.background.slots.size());
        for (const auto& slot : route.background.slots) {
            resolved_slots.push_back({parseSpeciesArchetype(slot.archetype), slot.count});
        }
    } else {
        // Backward-compat: synthesize from forager_count + predator_count.
        if (route.background.forager_count > 0) {
            resolved_slots.push_back({SpeciesArchetype::SmallForager, route.background.forager_count});
        }
        if (route.background.predator_count > 0) {
            resolved_slots.push_back({SpeciesArchetype::PackHunter, route.background.predator_count});
        }
    }

    // ---------------- background species init ----------------
    // One NeatEvolution + initial NEAT population per species slot.
    const std::size_t background_input_dim = backgroundSensoryDim(route);
    std::mt19937 init_rng(route.random_seed + 33333);

    struct SpeciesContext {
        SpeciesArchetype archetype;
        std::unique_ptr<NeatEvolution> evolution;
        std::vector<std::unique_ptr<IGenome>> population;
    };
    std::vector<SpeciesContext> species_contexts;
    species_contexts.reserve(resolved_slots.size());

    // Each population gets a wide ID stride to avoid cross-population collisions
    // after independent reproduce() calls (each population uses max_id+1 locally).
    // Protagonist: [1, kIdStride), Species[i]: [(i+1)*kIdStride, (i+2)*kIdStride)
    constexpr std::uint64_t kIdStride = 100000ULL;
    std::uint32_t evolution_seed_offset = 11111;
    for (std::size_t slot_idx = 0; slot_idx < resolved_slots.size(); ++slot_idx) {
        const auto& slot = resolved_slots[slot_idx];
        if (slot.count == 0) continue;
        SpeciesContext ctx;
        ctx.archetype = slot.archetype;
        ctx.evolution = make_neat_evolution(evolution_seed_offset);
        evolution_seed_offset += 13371;
        const auto output_dim = backgroundOutputDim(slot.archetype);
        const std::uint64_t id_base = static_cast<std::uint64_t>(slot_idx + 1) * kIdStride + 1ULL;
        ctx.population = createInitialNeatPopulation(
            slot.count,
            background_input_dim,
            output_dim,
            GenomeId{id_base},
            ctx.evolution->innovationTracker(),
            init_rng);
        species_contexts.push_back(std::move(ctx));
    }

    auto build_species_view = [&]() {
        std::vector<BackgroundSpecies> view;
        view.reserve(species_contexts.size());
        for (const auto& ctx : species_contexts) {
            BackgroundSpecies bs;
            bs.archetype = ctx.archetype;
            bs.population = &ctx.population;
            view.push_back(bs);
        }
        return view;
    };

    // ---------------- CSV setup ----------------
    std::ofstream evolution_metrics(run_dir / "evolution.csv", std::ios::out | std::ios::trunc);
    if (!evolution_metrics.is_open()) {
        LOG_ERROR("ProtagonistEcologyRuntime: failed to open evolution metrics output file");
        return 1;
    }
    evolution_metrics << "generation,min_fitness,avg_fitness,best_fitness";
    for (const auto& ctx : species_contexts) {
        const auto name = std::string(speciesArchetypeName(ctx.archetype));
        evolution_metrics << ',' << name << "_avg," << name << "_best";
    }
    evolution_metrics << '\n';

    std::ofstream fitness_csv(run_dir / "fitness.csv", std::ios::out | std::ios::trunc);
    fitness_csv << "generation,min_fitness,avg_fitness,best_fitness\n";

    std::ofstream behavior_csv(run_dir / "protagonist_behaviors.csv", std::ios::out | std::ios::trunc);
    if (!behavior_csv.is_open()) {
        LOG_ERROR("ProtagonistEcologyRuntime: failed to open protagonist behaviors output file");
        return 1;
    }
    behavior_csv << "generation,episodes,total_protagonists,survived_protagonists,deposits_total,deposits_stick,deposits_stone,"
                 << "completed_worksites,worksite_completion_events,active_fires,chop_attempts,chop_successes,"
                 << "craft_attempts,craft_successes,throw_attempts,throw_hits,hunt_attempts,hunt_successes,"
                 << "signal_emits,signal_co_attendance,cooperative_co_presence_ticks\n";

    std::ofstream species_csv(run_dir / "species_metrics.csv", std::ios::out | std::ios::trunc);
    if (!species_csv.is_open()) {
        LOG_ERROR("ProtagonistEcologyRuntime: failed to open species metrics output file");
        return 1;
    }
    species_csv << "generation,species,population,avg_fitness,best_fitness,min_fitness,avg_complexity\n";

    LOG_INFO("ProtagonistEcologyRuntime: training with {} background species: {}",
             species_contexts.size(),
             [&]() {
                 std::string s;
                 for (const auto& ctx : species_contexts) {
                     if (!s.empty()) s += ", ";
                     s += std::string(speciesArchetypeName(ctx.archetype)) + ":" + std::to_string(ctx.population.size());
                 }
                 return s;
             }());

    double best_fitness_seen = -std::numeric_limits<double>::infinity();
    for (std::size_t generation = 0; generation < params.generations; ++generation) {
        if (stop_requested_.load()) {
            break;
        }

        std::unordered_map<GenomeId, double> fitness_sum;
        std::vector<std::unordered_map<GenomeId, double>> species_fitness_sum(species_contexts.size());
        BehaviorAccumulator behavior;

        // Reset capability counters once before all parallel episodes.
        // Counters are atomic, so concurrent increments are safe;
        // total at end represents sum across all episodes (same semantics as before).
        ChopTreeCapability::resetCounters();
        CraftSpearCapability::resetCounters();
        ProtagonistHuntCapability::resetCounters();
        ThrowCarriedObjectCapability::resetCounters();

        // Per-episode results (each worker writes to its own slot, no contention).
        struct EpisodeResult {
            std::unordered_map<GenomeId, double> fitness_sum;
            std::vector<std::unordered_map<GenomeId, double>> species_fitness_sum;
            BehaviorAccumulator behavior;
        };
        std::vector<EpisodeResult> episode_results(episodes_per_generation);
        for (auto& r : episode_results) {
            r.species_fitness_sum.resize(species_contexts.size());
        }

        // Submit each episode to the thread pool.
        for (std::size_t episode = 0; episode < episodes_per_generation; ++episode) {
            evaluation_pool.enqueue([&, episode]() {
                if (stop_requested_.load()) return;

                auto generation_route = route;
                generation_route.random_seed = route.random_seed
                    + static_cast<std::uint32_t>(generation * 9973 + 17)
                    + static_cast<std::uint32_t>(episode * 131);
                auto world = createBootstrapWorld(generation_route);
                std::unique_ptr<TraceRecorder> trace_recorder;
                if (shouldTraceEpisode(trace_config, generation, episode)) {
                    const auto trace_dir = run_dir
                        / "trace"
                        / ("generation_" + std::to_string(generation))
                        / ("episode_" + std::to_string(episode));
                    trace_recorder = std::make_unique<TraceRecorder>(
                        *world,
                        trace_dir,
                        generation_route,
                        trace_config,
                        generation,
                        episode);
                }

                world->eventBus().publish(events::Event{
                    events::EventType::GenerationStarted,
                    events::GenerationStarted{static_cast<Generation>(generation)},
                    world->currentTick()});
                world->eventBus().flush();

                auto background_view = build_species_view();

                std::uint64_t local_next_agent_id = 1;
                seedTrainingPopulation(*world, generation_route, population, background_view, local_next_agent_id);

                EpisodeResult& result = episode_results[episode];
                evaluatePopulation(*world, params, population, background_view, result.behavior, stop_requested_, trace_recorder.get());
                accumulateGenomeFitness(*world, population, result.fitness_sum);
                for (std::size_t i = 0; i < background_view.size(); ++i) {
                    if (background_view[i].population != nullptr) {
                        accumulateGenomeFitness(*world, *background_view[i].population, result.species_fitness_sum[i]);
                    }
                }

                world->eventBus().flush();
            });
        }
        evaluation_pool.waitAll();

        // Merge per-episode results into the generation's accumulators.
        for (auto& r : episode_results) {
            for (const auto& [id, fit] : r.fitness_sum) {
                fitness_sum[id] += fit;
            }
            for (std::size_t i = 0; i < species_contexts.size(); ++i) {
                for (const auto& [id, fit] : r.species_fitness_sum[i]) {
                    species_fitness_sum[i][id] += fit;
                }
            }
            // Merge behavior counters.
            behavior.deposits_total += r.behavior.deposits_total;
            behavior.deposits_stick += r.behavior.deposits_stick;
            behavior.deposits_stone += r.behavior.deposits_stone;
            behavior.completed_worksites += r.behavior.completed_worksites;
            behavior.worksite_completion_events += r.behavior.worksite_completion_events;
            behavior.active_fires += r.behavior.active_fires;
            behavior.harvested_trees += r.behavior.harvested_trees;
            behavior.signal_emits += r.behavior.signal_emits;
            behavior.signal_co_attendance += r.behavior.signal_co_attendance;
            behavior.cooperative_co_presence_ticks += r.behavior.cooperative_co_presence_ticks;
            behavior.survived_protagonists += r.behavior.survived_protagonists;
            behavior.total_protagonists += r.behavior.total_protagonists;
        }
        // Capability counters are global atomics; read once at end (sum across episodes).
        behavior.chop_attempts = ChopTreeCapability::chop_attempts.load();
        behavior.chop_successes = ChopTreeCapability::chop_successes.load();
        behavior.craft_attempts = CraftSpearCapability::craft_attempts.load();
        behavior.craft_successes = CraftSpearCapability::craft_successes.load();
        behavior.throw_attempts = ThrowCarriedObjectCapability::throw_attempts.load();
        behavior.throw_hits = ThrowCarriedObjectCapability::throw_hits.load();
        behavior.hunt_attempts = ProtagonistHuntCapability::hunt_attempts.load();
        behavior.hunt_successes = ProtagonistHuntCapability::hunt_successes.load();

        const auto evaluated = collectEvaluated(population, fitness_sum, episodes_per_generation);
        const auto stats = computeStats(evaluated);

        std::vector<std::vector<EvaluatedIndividual>> species_evaluated;
        std::vector<GenerationStats> species_stats;
        species_evaluated.reserve(species_contexts.size());
        species_stats.reserve(species_contexts.size());

        auto avg_complexity = [](const std::vector<std::unique_ptr<IGenome>>& pop) {
            if (pop.empty()) return 0.0;
            double total = 0.0;
            std::size_t n = 0;
            for (const auto& g : pop) {
                if (g != nullptr) {
                    total += static_cast<double>(g->complexity());
                    ++n;
                }
            }
            return n == 0 ? 0.0 : total / static_cast<double>(n);
        };

        for (std::size_t i = 0; i < species_contexts.size(); ++i) {
            auto eval_i = collectEvaluated(species_contexts[i].population, species_fitness_sum[i], episodes_per_generation);
            const auto stat_i = computeStats(eval_i);
            species_stats.push_back(stat_i);
            species_evaluated.push_back(std::move(eval_i));
        }

        // ---- write evolution.csv ----
        evolution_metrics << generation << ','
                          << stats.min_fitness << ','
                          << stats.avg_fitness << ','
                          << stats.best_fitness;
        for (std::size_t i = 0; i < species_contexts.size(); ++i) {
            evolution_metrics << ',' << species_stats[i].avg_fitness << ',' << species_stats[i].best_fitness;
        }
        evolution_metrics << '\n';
        evolution_metrics.flush();

        // ---- write fitness.csv ----
        fitness_csv << generation << ','
                    << stats.min_fitness << ','
                    << stats.avg_fitness << ','
                    << stats.best_fitness << '\n';
        fitness_csv.flush();

        // ---- write protagonist_behaviors.csv ----
        behavior_csv << generation << ','
                     << episodes_per_generation << ','
                     << behavior.total_protagonists << ','
                     << behavior.survived_protagonists << ','
                     << behavior.deposits_total << ','
                     << behavior.deposits_stick << ','
                     << behavior.deposits_stone << ','
                     << behavior.completed_worksites << ','
                     << behavior.worksite_completion_events << ','
                     << behavior.active_fires << ','
                     << behavior.chop_attempts << ','
                     << behavior.chop_successes << ','
                     << behavior.craft_attempts << ','
                     << behavior.craft_successes << ','
                     << behavior.throw_attempts << ','
                     << behavior.throw_hits << ','
                     << behavior.hunt_attempts << ','
                     << behavior.hunt_successes << ','
                     << behavior.signal_emits << ','
                     << behavior.signal_co_attendance << ','
                     << behavior.cooperative_co_presence_ticks << '\n';
        behavior_csv.flush();

        // ---- write species_metrics.csv (one row per species) ----
        for (std::size_t i = 0; i < species_contexts.size(); ++i) {
            const auto cx_i = avg_complexity(species_contexts[i].population);
            species_csv << generation << ','
                        << speciesArchetypeName(species_contexts[i].archetype) << ','
                        << species_contexts[i].population.size() << ','
                        << species_stats[i].avg_fitness << ','
                        << species_stats[i].best_fitness << ','
                        << species_stats[i].min_fitness << ','
                        << cx_i << '\n';
        }
        species_csv.flush();

        // ---- log to stdout ----
        std::string species_log;
        for (std::size_t i = 0; i < species_contexts.size(); ++i) {
            if (!species_log.empty()) species_log += " | ";
            species_log += std::string(speciesArchetypeName(species_contexts[i].archetype))
                         + " avg=" + std::to_string(static_cast<int>(species_stats[i].avg_fitness * 100) / 100.0);
        }
        LOG_INFO("Gen {} | protagonist avg={:.2f} best={:.2f} | {} | builds={} chops={} hunts={} signals={}",
                 generation,
                 stats.avg_fitness,
                 stats.best_fitness,
                 species_log,
                 behavior.completed_worksites,
                 behavior.chop_successes,
                 behavior.hunt_successes,
                 behavior.signal_emits);

        // ---- save best protagonist genome ----
        if (!evaluated.empty()) {
            auto best_it = std::max_element(evaluated.begin(), evaluated.end(),
                [](const EvaluatedIndividual& a, const EvaluatedIndividual& b) {
                    return a.fitness < b.fitness;
                });
            const auto best_id = best_it->genome_id;
            for (const auto& genome : population) {
                if (genome != nullptr && genome->id() == best_id) {
                    if (auto* protagonist_genome = dynamic_cast<const ProtagonistGenome*>(genome.get()); protagonist_genome != nullptr) {
                        const auto gen_path = run_dir / ("best_genome_gen" + std::to_string(generation) + ".bin");
                        const auto latest_path = run_dir / "best_genome_latest.bin";
                        protagonist_genome->saveToFile(gen_path);
                        protagonist_genome->saveToFile(latest_path);
                        if (best_it->fitness > best_fitness_seen) {
                            best_fitness_seen = best_it->fitness;
                            protagonist_genome->saveToFile(run_dir / "best_genome_overall.bin");
                        }
                    }
                    break;
                }
            }
        }

        // ---- checkpoint protagonists ----
        if (params.checkpoint_every > 0
            && (generation + 1) % params.checkpoint_every == 0) {
            const auto checkpoint_dir = run_dir / ("checkpoint_gen" + std::to_string(generation));
            std::error_code ec;
            std::filesystem::create_directories(checkpoint_dir, ec);
            std::size_t saved = 0;
            std::vector<std::string> saved_files;
            saved_files.reserve(population.size());
            for (const auto& genome : population) {
                if (genome == nullptr) continue;
                if (auto* protagonist_genome = dynamic_cast<const ProtagonistGenome*>(genome.get())) {
                    const auto filename = "genome_" + std::to_string(genome->id().value) + ".bin";
                    const auto path = checkpoint_dir / filename;
                    if (protagonist_genome->saveToFile(path)) {
                        ++saved;
                        saved_files.push_back(filename);
                    }
                }
            }
            std::ofstream manifest(checkpoint_dir / "checkpoint_manifest.json", std::ios::out | std::ios::trunc);
            if (manifest.is_open()) {
                manifest << "{\n"
                         << "  \"schema_version\": 1,\n"
                         << "  \"generation\": " << generation << ",\n"
                         << "  \"scenario_name\": \"" << jsonEscape(route.scenario_name) << "\",\n"
                         << "  \"world_role\": \"" << jsonEscape(route.world_role) << "\",\n"
                         << "  \"config_path\": \"" << jsonEscape(config_.path().string()) << "\",\n"
                         << "  \"resume_supported\": \"protagonist_population_only\",\n"
                         << "  \"background_species_resume\": false,\n"
                         << "  \"rng_state_resume\": false,\n"
                         << "  \"innovation_tracker_resume\": false,\n"
                         << "  \"protagonist_population_count\": " << saved << ",\n"
                         << "  \"protagonist_genomes\": [";
                for (std::size_t i = 0; i < saved_files.size(); ++i) {
                    if (i > 0) manifest << ", ";
                    manifest << "\"" << jsonEscape(saved_files[i]) << "\"";
                }
                manifest << "]\n}\n";
            }
            LOG_INFO("Protagonist gen {} checkpoint saved {} genomes -> {}", generation, saved, checkpoint_dir.string());
        }

        if (generation + 1 >= params.generations || stop_requested_.load()) {
            break;
        }

        // ---- reproduce all populations ----
        auto raw = rawGenomePointers(population);
        population = evolution.reproduce(evaluated, raw, nullptr);

        for (std::size_t i = 0; i < species_contexts.size(); ++i) {
            if (species_contexts[i].population.empty()) continue;
            auto raw_i = rawGenomePointers(species_contexts[i].population);
            species_contexts[i].population = species_contexts[i].evolution->reproduce(species_evaluated[i], raw_i, nullptr);
        }
    }

    LOG_INFO("ProtagonistEcologyRuntime: training run complete at {}", run_dir.string());
    return 0;
}

void ProtagonistEcologyRuntime::stop() {
    stop_requested_.store(true);
}

}  // namespace neuro::routes::protagonist
