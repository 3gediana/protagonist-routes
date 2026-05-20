#include "runtime/TraceRecorder.h"

#include "brain/ProtagonistBrain.h"
#include "core/agent/Agent.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/events/EventType.h"
#include "core/world/World2D.h"
#include "core/world/layers/AgentLayer.h"
#include "core/world/layers/ResourceLayer.h"
#include "core/world/layers/object/ObjectLayer.h"
#include "world/CampfireLayer.h"
#include "world/NurseryLogisticsLayer.h"
#include "world/SurvivalStatusLayer.h"
#include "world/WorksiteLayer.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

namespace neuro::routes::protagonist {

namespace {

std::string jsonEscape(const std::string& input) {
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

const char* objectTypeName(ObjectType type) {
    switch (type) {
        case ObjectType::Stone: return "stone";
        case ObjectType::Stick: return "stick";
        case ObjectType::Spear: return "spear";
        case ObjectType::Count: return "count";
    }
    return "unknown";
}

std::size_t availableResourceCount(const ResourceLayer* resources) {
    if (resources == nullptr) {
        return 0;
    }
    std::size_t count = 0;
    for (const auto& resource : resources->resources()) {
        if (resource.available && resource.amount > 0.0f) {
            ++count;
        }
    }
    return count;
}

std::size_t objectCountByType(const ObjectLayer* objects, ObjectType type) {
    if (objects == nullptr) {
        return 0;
    }
    std::size_t count = 0;
    for (const auto& object : objects->objects()) {
        if (!object.carried && object.type == type) {
            ++count;
        }
    }
    return count;
}

float averageValue(const std::vector<float>& values) {
    if (values.empty()) {
        return 0.0f;
    }
    float sum = 0.0f;
    for (const float value : values) {
        sum += value;
    }
    return sum / static_cast<float>(values.size());
}

float maxValue(const std::vector<float>& values) {
    if (values.empty()) {
        return 0.0f;
    }
    return *std::max_element(values.begin(), values.end());
}

float l2Norm(const std::vector<float>& values) {
    float sum = 0.0f;
    for (const float value : values) {
        sum += value * value;
    }
    return std::sqrt(sum);
}

}  // namespace

TraceRecorderConfig loadTraceRecorderConfig(const config::Config& config) {
    TraceRecorderConfig out;
    out.enabled = config.getOr<bool>("trace.enabled", out.enabled);
    out.events_enabled = config.getOr<bool>("trace.events_enabled", out.events_enabled);
    out.agents_enabled = config.getOr<bool>("trace.agents_enabled", out.agents_enabled);
    out.world_enabled = config.getOr<bool>("trace.world_enabled", out.world_enabled);
    out.agent_interval = std::max<std::size_t>(1, config.getOr<std::size_t>("trace.agent_interval", out.agent_interval));
    out.world_interval = std::max<std::size_t>(1, config.getOr<std::size_t>("trace.world_interval", out.world_interval));
    out.max_generations = config.getOr<std::size_t>("trace.max_generations", out.max_generations);
    out.max_episodes_per_generation = config.getOr<std::size_t>("trace.max_episodes_per_generation", out.max_episodes_per_generation);
    return out;
}

bool shouldTraceEpisode(const TraceRecorderConfig& config, std::size_t generation, std::size_t episode) {
    return config.enabled
        && generation < config.max_generations
        && episode < config.max_episodes_per_generation;
}

TraceRecorder::TraceRecorder(World2D& world,
                             std::filesystem::path trace_dir,
                             const ProtagonistEcologyConfig& route,
                             const TraceRecorderConfig& config,
                             std::size_t generation,
                             std::size_t episode)
    : world_(world), config_(config), trace_dir_(std::move(trace_dir)) {
    if (!config_.enabled) {
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories(trace_dir_, ec);
    if (ec) {
        return;
    }

    if (config_.events_enabled) {
        event_trace_.open(trace_dir_ / "event_trace.jsonl", std::ios::out | std::ios::trunc);
    }
    if (config_.agents_enabled) {
        agent_trace_.open(trace_dir_ / "agent_trace.csv", std::ios::out | std::ios::trunc);
        if (agent_trace_.is_open()) {
            agent_trace_ << "step_index,time_seconds,agent_id,genome_id,species_id,x,y,vx,vy,alive,energy,age_seconds,fitness,carried_object,is_carrying,health_ratio,hydration_ratio,infected,immune,immune_memory_count,food_eaten,plant_food,meat_food,visited_cells,damage_dealt,kills,goal_index,goal_name,goal_commitment,goal_elapsed_seconds,goal_progress,episodic_event_count,episodic_top_type,episodic_top_salience,social_relation_count,social_best_coop,social_best_comm,social_best_hostility,spatial_water_cells,spatial_food_cells,spatial_danger_cells,spatial_fire_cells,spatial_worksite_cells,dnc_usage_mean,dnc_usage_max,dnc_write_peak,dnc_read_peak,dnc_read_norm,dnc_precedence_peak,dnc_write_focus_slot,dnc_read_focus_slot\n";
        }
    }
    if (config_.world_enabled) {
        world_trace_.open(trace_dir_ / "world_trace.csv", std::ios::out | std::ios::trunc);
        if (world_trace_.is_open()) {
            world_trace_ << "tick,time_seconds,total_agents,living_agents,available_resources,loose_stones,loose_sticks,loose_spears,stockpiled_total,stockpiled_sticks,stockpiled_stones,active_fires,active_worksites,completed_worksites,avg_health_ratio,avg_hydration_ratio,dehydrated_agents,injured_agents\n";
        }
    }

    writeManifest(route, generation, episode);

    if (event_trace_.is_open()) {
        event_subscription_ = world_.eventBus().subscribeAny([this](const events::Event& event) {
            writeEvent(event);
        });
    }
}

TraceRecorder::~TraceRecorder() {
    if (event_subscription_ != 0) {
        world_.eventBus().unsubscribe(event_subscription_);
    }
}

bool TraceRecorder::active() const {
    return config_.enabled;
}

void TraceRecorder::recordTick(World2D& world) {
    const auto tick = world.currentTick();
    if (agent_trace_.is_open() && tick % config_.agent_interval == 0) {
        writeAgentSnapshot(world);
    }
    if (world_trace_.is_open() && tick % config_.world_interval == 0) {
        writeWorldSnapshot(world);
    }
}

void TraceRecorder::writeManifest(const ProtagonistEcologyConfig& route, std::size_t generation, std::size_t episode) {
    std::ofstream manifest(trace_dir_ / "replay_manifest.json", std::ios::out | std::ios::trunc);
    if (!manifest.is_open()) {
        return;
    }
    manifest << "{\n"
             << "  \"schema_version\": 1,\n"
             << "  \"generation\": " << generation << ",\n"
             << "  \"episode\": " << episode << ",\n"
             << "  \"world_role\": \"" << jsonEscape(route.world_role) << "\",\n"
             << "  \"scenario_name\": \"" << jsonEscape(route.scenario_name) << "\",\n"
             << "  \"random_seed\": " << route.random_seed << ",\n"
             << "  \"world_size\": [" << route.world_size.x << ", " << route.world_size.y << "],\n"
             << "  \"agent_interval\": " << config_.agent_interval << ",\n"
             << "  \"world_interval\": " << config_.world_interval << "\n"
             << "}\n";
}

void TraceRecorder::writeEvent(const events::Event& event) {
    if (!event_trace_.is_open()) {
        return;
    }

    event_trace_ << "{\"tick\":" << event.tick()
                 << ",\"time_seconds\":" << event.timeSeconds()
                 << ",\"type\":\"" << events::toString(event.type()) << "\"";

    switch (event.type()) {
        case events::EventType::GenerationStarted: {
            const auto& payload = event.payloadAs<events::GenerationStarted>();
            event_trace_ << ",\"generation\":" << payload.gen;
            break;
        }
        case events::EventType::GenerationEnded: {
            const auto& payload = event.payloadAs<events::GenerationEnded>();
            event_trace_ << ",\"generation\":" << payload.gen
                         << ",\"best_fitness\":" << payload.best_fitness
                         << ",\"avg_fitness\":" << payload.avg_fitness
                         << ",\"min_fitness\":" << payload.min_fitness;
            break;
        }
        case events::EventType::AgentBorn: {
            const auto& payload = event.payloadAs<events::AgentBorn>();
            event_trace_ << ",\"agent_id\":" << payload.id.value
                         << ",\"genome_id\":" << payload.genome.value
                         << ",\"x\":" << payload.pos.x
                         << ",\"y\":" << payload.pos.y;
            break;
        }
        case events::EventType::AgentDied: {
            const auto& payload = event.payloadAs<events::AgentDied>();
            event_trace_ << ",\"agent_id\":" << payload.id.value
                         << ",\"cause\":\"" << jsonEscape(payload.cause) << "\"";
            break;
        }
        case events::EventType::AgentMoved: {
            const auto& payload = event.payloadAs<events::AgentMoved>();
            event_trace_ << ",\"agent_id\":" << payload.id.value
                         << ",\"from_x\":" << payload.from.x
                         << ",\"from_y\":" << payload.from.y
                         << ",\"to_x\":" << payload.to.x
                         << ",\"to_y\":" << payload.to.y;
            break;
        }
        case events::EventType::AgentAteFood: {
            const auto& payload = event.payloadAs<events::AgentAteFood>();
            event_trace_ << ",\"agent_id\":" << payload.id.value
                         << ",\"x\":" << payload.pos.x
                         << ",\"y\":" << payload.pos.y
                         << ",\"amount\":" << payload.amount;
            break;
        }
        case events::EventType::AgentEnergyChanged: {
            const auto& payload = event.payloadAs<events::AgentEnergyChanged>();
            event_trace_ << ",\"agent_id\":" << payload.id.value
                         << ",\"old_energy\":" << payload.old_energy
                         << ",\"new_energy\":" << payload.new_energy;
            break;
        }
        case events::EventType::ResourceSpawned: {
            const auto& payload = event.payloadAs<events::ResourceSpawned>();
            event_trace_ << ",\"x\":" << payload.pos.x
                         << ",\"y\":" << payload.pos.y
                         << ",\"amount\":" << payload.amount;
            break;
        }
        case events::EventType::ResourceConsumed: {
            const auto& payload = event.payloadAs<events::ResourceConsumed>();
            event_trace_ << ",\"x\":" << payload.pos.x
                         << ",\"y\":" << payload.pos.y;
            break;
        }
        case events::EventType::DecisionTriggered: {
            const auto& payload = event.payloadAs<events::DecisionTriggered>();
            event_trace_ << ",\"agent_id\":" << payload.id.value
                         << ",\"decision_tick\":" << payload.tick
                         << ",\"decision_time_seconds\":" << event.timeSeconds();
            break;
        }
        case events::EventType::TerrainGenerated: {
            const auto& payload = event.payloadAs<events::TerrainGenerated>();
            event_trace_ << ",\"seed\":" << payload.seed;
            break;
        }
        case events::EventType::AgentEnteredCave: {
            const auto& payload = event.payloadAs<events::AgentEnteredCave>();
            event_trace_ << ",\"agent_id\":" << payload.id.value
                         << ",\"x\":" << payload.pos.x
                         << ",\"y\":" << payload.pos.y;
            break;
        }
        case events::EventType::AgentLeftCave: {
            const auto& payload = event.payloadAs<events::AgentLeftCave>();
            event_trace_ << ",\"agent_id\":" << payload.id.value
                         << ",\"x\":" << payload.pos.x
                         << ",\"y\":" << payload.pos.y;
            break;
        }
        case events::EventType::TerrainModified: {
            const auto& payload = event.payloadAs<events::TerrainModified>();
            event_trace_ << ",\"agent_id\":" << payload.id.value
                         << ",\"x\":" << payload.pos.x
                         << ",\"y\":" << payload.pos.y
                         << ",\"dig\":" << payload.dig
                         << ",\"build\":" << payload.build;
            break;
        }
        case events::EventType::AgentEmittedSignal: {
            const auto& payload = event.payloadAs<events::AgentEmittedSignal>();
            event_trace_ << ",\"agent_id\":" << payload.id.value
                         << ",\"x\":" << payload.pos.x
                         << ",\"y\":" << payload.pos.y
                         << ",\"channel\":" << static_cast<int>(payload.channel)
                         << ",\"intensity\":" << payload.intensity;
            break;
        }
        case events::EventType::AgentAttacked: {
            const auto& payload = event.payloadAs<events::AgentAttacked>();
            event_trace_ << ",\"attacker\":" << payload.attacker.value
                         << ",\"victim\":" << payload.victim.value
                         << ",\"damage\":" << payload.damage;
            break;
        }
        case events::EventType::AgentFled: {
            const auto& payload = event.payloadAs<events::AgentFled>();
            event_trace_ << ",\"agent_id\":" << payload.id.value
                         << ",\"from_x\":" << payload.from.x
                         << ",\"from_y\":" << payload.from.y
                         << ",\"to_x\":" << payload.to.x
                         << ",\"to_y\":" << payload.to.y
                         << ",\"intensity\":" << payload.intensity;
            break;
        }
        case events::EventType::ObjectPickedUp: {
            const auto& payload = event.payloadAs<events::ObjectPickedUp>();
            event_trace_ << ",\"agent_id\":" << payload.id.value
                         << ",\"object_id\":" << payload.object_id;
            break;
        }
        case events::EventType::ObjectDropped: {
            const auto& payload = event.payloadAs<events::ObjectDropped>();
            event_trace_ << ",\"agent_id\":" << payload.id.value
                         << ",\"object_id\":" << payload.object_id;
            break;
        }
        case events::EventType::MatingOccurred: {
            const auto& payload = event.payloadAs<events::MatingOccurred>();
            event_trace_ << ",\"initiator\":" << payload.initiator.value
                         << ",\"partner\":" << payload.partner.value;
            break;
        }
        case events::EventType::InterventionTriggered: {
            const auto& payload = event.payloadAs<events::InterventionTriggered>();
            event_trace_ << ",\"kind\":\"" << jsonEscape(payload.kind) << "\""
                         << ",\"user\":\"" << jsonEscape(payload.user) << "\"";
            break;
        }
        default:
            break;
    }

    event_trace_ << "}\n";
}

void TraceRecorder::writeAgentSnapshot(World2D& world) {
    auto* agents = world.getLayer<AgentLayer>();
    auto* survival = world.getLayer<SurvivalStatusLayer>();
    if (agents == nullptr) {
        return;
    }

    for (const auto* agent : agents->agents()) {
        if (agent == nullptr) {
            continue;
        }
        const auto pos = agent->position();
        const auto vel = agent->velocity();
        const float health = survival != nullptr ? survival->healthRatio(agent->id()) : 1.0f;
        const float hydration = survival != nullptr ? survival->hydrationRatio(agent->id()) : 1.0f;
        int goal_index = -1;
        const char* goal_name = "na";
        float goal_commitment = 0.0f;
        float goal_elapsed_seconds = 0.0f;
        float goal_progress = 0.0f;
        std::size_t episodic_event_count = 0;
        std::string episodic_top_type = "none";
        float episodic_top_salience = 0.0f;
        std::size_t social_relation_count = 0;
        float social_best_coop = 0.0f;
        float social_best_comm = 0.0f;
        float social_best_hostility = 0.0f;
        std::size_t spatial_water_cells = 0;
        std::size_t spatial_food_cells = 0;
        std::size_t spatial_danger_cells = 0;
        std::size_t spatial_fire_cells = 0;
        std::size_t spatial_worksite_cells = 0;
        float dnc_usage_mean = 0.0f;
        float dnc_usage_max = 0.0f;
        float dnc_write_peak = 0.0f;
        float dnc_read_peak = 0.0f;
        float dnc_read_norm = 0.0f;
        float dnc_precedence_peak = 0.0f;
        float dnc_write_focus_slot = 0.0f;
        float dnc_read_focus_slot = 0.0f;

        if (const auto* protagonist_brain = dynamic_cast<const ProtagonistBrain*>(&agent->brain()); protagonist_brain != nullptr) {
            const auto dnc = protagonist_brain->dncSummary();
            const auto& goal = protagonist_brain->goalMemory();
            goal_index = static_cast<int>(goal.currentGoal());
            goal_name = goalTypeName(goal.currentGoal());
            goal_commitment = goal.commitment();
            goal_elapsed_seconds = goal.elapsedSeconds();
            goal_progress = goal.completionProgress();

            const auto& episodic = protagonist_brain->episodicMemory();
            episodic_event_count = episodic.events().size();
            const auto top_events = episodic.topSalient(1);
            if (!top_events.empty()) {
                episodic_top_type = top_events.front().type;
                episodic_top_salience = top_events.front().salience;
            }

            const auto& social = protagonist_brain->socialMemory();
            social_relation_count = social.relations().size();
            for (const auto& [_, rel] : social.relations()) {
                social_best_coop = std::max(social_best_coop, rel.cooperation_score);
                social_best_comm = std::max(social_best_comm, rel.communication_score);
                social_best_hostility = std::max(social_best_hostility, rel.hostility_score);
            }

            const auto& cells = protagonist_brain->spatialMemory().cells();
            for (const auto& cell : cells) {
                spatial_water_cells += cell.water_seen >= 0.15f ? 1u : 0u;
                spatial_food_cells += cell.food_seen >= 0.15f ? 1u : 0u;
                spatial_danger_cells += cell.danger_seen >= 0.15f ? 1u : 0u;
                spatial_fire_cells += cell.fire_seen >= 0.15f ? 1u : 0u;
                spatial_worksite_cells += cell.worksite_seen >= 0.15f ? 1u : 0u;
            }

            dnc_usage_mean = dnc.usage_mean;
            dnc_usage_max = dnc.usage_max;
            dnc_write_peak = dnc.write_peak;
            dnc_read_peak = dnc.read_peak;
            dnc_read_norm = dnc.read_norm;
            dnc_precedence_peak = dnc.precedence_peak;
            dnc_write_focus_slot = dnc.write_focus_slot;
            dnc_read_focus_slot = dnc.read_focus_slot;
        }

        agent_trace_ << world.currentTick() << ','
                     << world.currentTimeSeconds() << ','
                     << agent->id().value << ','
                     << agent->genomeId().value << ','
                     << agent->speciesId().value << ','
                     << pos.x << ',' << pos.y << ','
                     << vel.x << ',' << vel.y << ','
                     << (agent->alive() ? 1 : 0) << ','
                     << agent->energy() << ','
                     << agent->ageSeconds() << ','
                     << agent->fitness() << ','
                     << agent->carriedObject() << ','
                     << (agent->isCarrying() ? 1 : 0) << ','
                     << health << ','
                     << hydration << ','
                     << (agent->infected() ? 1 : 0) << ','
                     << (agent->immune() ? 1 : 0) << ','
                     << agent->immuneMemory().size() << ','
                     << agent->foodEatenThisTick() << ','
                     << agent->plantFoodThisTick() << ','
                     << agent->meatFoodThisTick() << ','
                     << agent->visitedCellCount() << ','
                     << agent->damageDealt() << ','
                     << agent->kills() << ','
                     << goal_index << ','
                     << goal_name << ','
                     << goal_commitment << ','
                     << goal_elapsed_seconds << ','
                     << goal_progress << ','
                     << episodic_event_count << ','
                     << episodic_top_type << ','
                     << episodic_top_salience << ','
                     << social_relation_count << ','
                     << social_best_coop << ','
                     << social_best_comm << ','
                     << social_best_hostility << ','
                     << spatial_water_cells << ','
                     << spatial_food_cells << ','
                     << spatial_danger_cells << ','
                     << spatial_fire_cells << ','
                     << spatial_worksite_cells << ','
                     << dnc_usage_mean << ','
                     << dnc_usage_max << ','
                     << dnc_write_peak << ','
                     << dnc_read_peak << ','
                     << dnc_read_norm << ','
                     << dnc_precedence_peak << ','
                     << dnc_write_focus_slot << ','
                     << dnc_read_focus_slot << '\n';
    }
    agent_trace_.flush();
}

void TraceRecorder::writeWorldSnapshot(World2D& world) {
    auto* agents = world.getLayer<AgentLayer>();
    auto* resources = world.getLayer<ResourceLayer>();
    auto* objects = world.getLayer<ObjectLayer>();
    auto* logistics = world.getLayer<NurseryLogisticsLayer>();
    auto* campfires = world.getLayer<CampfireLayer>();
    auto* worksites = world.getLayer<WorksiteLayer>();
    auto* survival = world.getLayer<SurvivalStatusLayer>();

    world_trace_ << world.currentTick() << ','
                 << world.currentTimeSeconds() << ','
                 << (agents != nullptr ? agents->size() : 0) << ','
                 << (agents != nullptr ? agents->livingCount() : 0) << ','
                 << availableResourceCount(resources) << ','
                 << objectCountByType(objects, ObjectType::Stone) << ','
                 << objectCountByType(objects, ObjectType::Stick) << ','
                 << objectCountByType(objects, ObjectType::Spear) << ','
                 << (logistics != nullptr ? logistics->currentStockpiledCount() : 0) << ','
                 << (logistics != nullptr ? logistics->stockpiledCount(ObjectType::Stick) : 0) << ','
                 << (logistics != nullptr ? logistics->stockpiledCount(ObjectType::Stone) : 0) << ','
                 << (campfires != nullptr ? campfires->activeFireCount() : 0) << ','
                 << (worksites != nullptr ? worksites->activeSiteCount() : 0) << ','
                 << (worksites != nullptr ? worksites->completedSiteCount() : 0) << ','
                 << (survival != nullptr ? survival->averageHealthRatio() : 1.0f) << ','
                 << (survival != nullptr ? survival->averageHydrationRatio() : 1.0f) << ','
                 << (survival != nullptr ? survival->dehydratedCount() : 0) << ','
                 << (survival != nullptr ? survival->injuredCount() : 0) << '\n';
    world_trace_.flush();
}

}  // namespace neuro::routes::protagonist
