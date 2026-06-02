#include "runtime/TraceRecorder.h"

#include "brain/ProtagonistBrain.h"
#include "core/agent/Agent.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/events/EventType.h"
#include "core/logging/Logger.h"
#include "core/world/World2D.h"
#include "core/world/layers/AgentLayer.h"
#include "core/world/layers/ResourceLayer.h"
#include "core/world/layers/SeasonLayer.h"
#include "core/world/layers/TimeOfDayLayer.h"
#include "core/world/layers/WeatherLayer.h"
#include "core/world/layers/object/ObjectLayer.h"
#include "core/world/layers/terrain/TerrainLayer.h"
#include "world/CampfireLayer.h"
#include "world/WaterLayer.h"
#include "world/NurseryLogisticsLayer.h"
#include "world/SurvivalStatusLayer.h"
#include "world/TreeLayer.h"
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

// 2026-05-28 disk-budget: returns true if `stream` has crossed `cap` bytes
// (via tellp()) and the caller should skip the upcoming write. On the first
// crossing for a given stream we close it (so subsequent calls become cheap
// no-ops via is_open()==false) and emit a single LOG_WARN. `cap == 0`
// disables the guard entirely. Mirrors the D-044 ledger / communication /
// signal-debug CSV pattern.
bool checkAndCapStream(std::ofstream& stream,
                       bool& warned_flag,
                       const std::filesystem::path& path,
                       std::size_t cap_bytes) {
    if (!stream.is_open()) {
        return true;  // already closed -> caller should skip
    }
    if (cap_bytes == 0) {
        return false;  // cap disabled
    }
    const auto pos = stream.tellp();
    if (pos < 0) {
        return false;  // tellp() failed; let the write proceed
    }
    if (static_cast<std::size_t>(pos) < cap_bytes) {
        return false;
    }
    // Reached cap: close stream + warn once.
    stream.close();
    if (!warned_flag) {
        warned_flag = true;
        LOG_WARN("TraceRecorder: closed {} after reaching disk-budget cap {} MB (D-044 parity guard); "
                 "adjust trace.max_bytes_per_file or reduce trace.max_generations to silence",
                 path.string(),
                 cap_bytes / (1024ULL * 1024ULL));
    }
    return true;
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
    // 2026-05-29 trajectory-analysis pivot: trace.tail_generations.
    // Default 0 keeps legacy behavior (first-N gens only).
    out.tail_generations = config.getOr<std::size_t>("trace.tail_generations", out.tail_generations);
    // L7 R1: viewer frame JSON
    out.viewer_frames_enabled = config.getOr<bool>("trace.viewer_frames_enabled", out.viewer_frames_enabled);
    out.viewer_frame_interval = std::max<std::size_t>(1, config.getOr<std::size_t>("trace.viewer_frame_interval", out.viewer_frame_interval));
    // 2026-05-28 disk-budget: per-file cap. 0 disables. Defaults to 200 MB
    // (D-044 parity). Override with `trace.max_bytes_per_file = N`.
    out.max_bytes_per_file = config.getOr<std::size_t>("trace.max_bytes_per_file", out.max_bytes_per_file);
    return out;
}

bool shouldTraceEpisode(const TraceRecorderConfig& config, std::size_t generation, std::size_t episode) {
    return config.enabled
        && generation < config.max_generations
        && episode < config.max_episodes_per_generation;
}

bool shouldTraceEpisode(const TraceRecorderConfig& config,
                        std::size_t generation,
                        std::size_t episode,
                        std::size_t total_generations) {
    if (!config.enabled) return false;
    if (episode >= config.max_episodes_per_generation) return false;
    if (generation < config.max_generations) return true;
    // Tail mode: include the last `tail_generations` gens. We guard
    // against underflow when `total_generations < tail_generations`
    // (e.g. very short smoke runs that opt into tail-mode anyway).
    if (config.tail_generations == 0) return false;
    if (total_generations == 0) return false;
    const std::size_t tail_start = (config.tail_generations >= total_generations)
                                       ? 0
                                       : total_generations - config.tail_generations;
    return generation >= tail_start;
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
            agent_trace_ << "step_index,time_seconds,agent_id,genome_id,species_id,x,y,vx,vy,alive,energy,age_seconds,fitness,carried_object,is_carrying,health_ratio,hydration_ratio,infected,immune,immune_memory_count,food_eaten,plant_food,meat_food,visited_cells,damage_dealt,kills,goal_index,goal_name,goal_commitment,goal_elapsed_seconds,goal_progress,episodic_event_count,episodic_top_type,episodic_top_salience,social_relation_count,social_best_coop,social_best_comm,social_best_hostility,spatial_water_cells,spatial_food_cells,spatial_danger_cells,spatial_fire_cells,spatial_worksite_cells,dnc_usage_mean,dnc_usage_max,dnc_write_peak,dnc_read_peak,dnc_read_norm,dnc_precedence_peak,dnc_write_focus_slot,dnc_read_focus_slot,recent_signal_dx,recent_signal_dy,recent_signal_salience,recent_signal_age_seconds,nearest_food_dist,predator_nearby_count,worksite_distance,season,season_progress,time_of_day,light_level,weather,core_temperature,hunger\n";
        }
    }
    if (config_.world_enabled) {
        world_trace_.open(trace_dir_ / "world_trace.csv", std::ios::out | std::ios::trunc);
        if (world_trace_.is_open()) {
            world_trace_ << "tick,time_seconds,total_agents,living_agents,available_resources,loose_stones,loose_sticks,loose_spears,stockpiled_total,stockpiled_sticks,stockpiled_stones,active_fires,active_worksites,completed_worksites,avg_health_ratio,avg_hydration_ratio,dehydrated_agents,injured_agents\n";
        }
    }
    if (config_.viewer_frames_enabled) {
        viewer_frames_.open(trace_dir_ / "viewer_frames.jsonl", std::ios::out | std::ios::trunc);
        writeViewerWorld(world_, route);
    }

    writeManifest(route, generation, episode);

    if (event_trace_.is_open()) {
        event_subscription_ = world_.eventBus().subscribeAny([this](const events::Event& event) {
            // checkAndCapStream() inside writeEvent() short-circuits once the
            // event_trace_ stream is closed (i.e. cap reached), so callbacks
            // remain cheap after the cap kicks in.
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
    if (viewer_frames_.is_open() && tick % config_.viewer_frame_interval == 0) {
        writeViewerFrame(world);
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
    if (checkAndCapStream(event_trace_, event_cap_warned_,
                          trace_dir_ / "event_trace.jsonl",
                          config_.max_bytes_per_file)) {
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
        case events::EventType::ChopStarted: {
            const auto& payload = event.payloadAs<events::ChopStarted>();
            event_trace_ << ",\"chopper\":" << payload.chopper.value
                         << ",\"tree_id\":" << payload.tree_id
                         << ",\"x\":" << payload.tree_pos.x
                         << ",\"y\":" << payload.tree_pos.y
                         << ",\"started_at_seconds\":" << payload.started_at_seconds;
            break;
        }
        case events::EventType::ChopCompleted: {
            const auto& payload = event.payloadAs<events::ChopCompleted>();
            event_trace_ << ",\"chopper\":" << payload.chopper.value
                         << ",\"tree_id\":" << payload.tree_id
                         << ",\"x\":" << payload.tree_pos.x
                         << ",\"y\":" << payload.tree_pos.y
                         << ",\"started_at_seconds\":" << payload.started_at_seconds
                         << ",\"duration_seconds\":" << payload.duration_seconds
                         << ",\"wood_yield\":" << payload.wood_yield;
            break;
        }
        case events::EventType::ChopAbandoned: {
            const auto& payload = event.payloadAs<events::ChopAbandoned>();
            event_trace_ << ",\"chopper\":" << payload.chopper.value
                         << ",\"tree_id\":" << payload.tree_id
                         << ",\"started_at_seconds\":" << payload.started_at_seconds
                         << ",\"duration_seconds\":" << payload.duration_seconds
                         << ",\"remaining_integrity\":" << payload.remaining_integrity;
            break;
        }
        case events::EventType::BuildStarted: {
            const auto& payload = event.payloadAs<events::BuildStarted>();
            event_trace_ << ",\"builder\":" << payload.builder.value
                         << ",\"worksite_id\":" << payload.worksite_id
                         << ",\"x\":" << payload.site_pos.x
                         << ",\"y\":" << payload.site_pos.y
                         << ",\"started_at_seconds\":" << payload.started_at_seconds;
            break;
        }
        case events::EventType::BuildCompleted: {
            const auto& payload = event.payloadAs<events::BuildCompleted>();
            event_trace_ << ",\"worksite_id\":" << payload.worksite_id
                         << ",\"x\":" << payload.site_pos.x
                         << ",\"y\":" << payload.site_pos.y
                         << ",\"started_at_seconds\":" << payload.started_at_seconds
                         << ",\"duration_seconds\":" << payload.duration_seconds
                         << ",\"contributors\":[";
            for (std::size_t i = 0; i < payload.contributors.size(); ++i) {
                if (i != 0) event_trace_ << ',';
                event_trace_ << payload.contributors[i].value;
            }
            event_trace_ << "],\"contribution_seconds\":[";
            for (std::size_t i = 0; i < payload.contribution_seconds.size(); ++i) {
                if (i != 0) event_trace_ << ',';
                event_trace_ << payload.contribution_seconds[i];
            }
            event_trace_ << ']';
            break;
        }
        case events::EventType::WorksiteMaterialDeposited: {
            const auto& payload = event.payloadAs<events::WorksiteMaterialDeposited>();
            event_trace_ << ",\"depositor\":" << payload.depositor.value
                         << ",\"worksite_id\":" << payload.worksite_id
                         << ",\"material_type\":" << static_cast<int>(payload.material_type);
            break;
        }
        case events::EventType::CreditAssigned: {
            const auto& payload = event.payloadAs<events::CreditAssigned>();
            event_trace_ << ",\"recipient\":" << payload.recipient.value
                         << ",\"source_kind\":" << static_cast<int>(payload.source_kind)
                         << ",\"source_id\":" << payload.source_id
                         << ",\"share\":" << payload.share
                         << ",\"reward\":" << payload.reward;
            break;
        }
        default:
            break;
    }

    event_trace_ << "}\n";
}

void TraceRecorder::writeAgentSnapshot(World2D& world) {
    if (checkAndCapStream(agent_trace_, agent_cap_warned_,
                          trace_dir_ / "agent_trace.csv",
                          config_.max_bytes_per_file)) {
        return;
    }
    auto* agents = world.getLayer<AgentLayer>();
    auto* survival = world.getLayer<SurvivalStatusLayer>();
    auto* resources = world.getLayer<ResourceLayer>();
    auto* worksites = world.getLayer<WorksiteLayer>();
    if (agents == nullptr) {
        return;
    }
    // Realism Phase B (2026-05-30): ground-truth ambient environment state,
    // recorded so env-responsiveness can be measured against true season/
    // weather/time-of-day instead of being reconstructed from episode time.
    // World-global per snapshot; computed once outside the agent loop.
    auto* season_layer = world.getLayer<SeasonLayer>();
    auto* tod_layer = world.getLayer<TimeOfDayLayer>();
    auto* weather_layer = world.getLayer<WeatherLayer>();
    const int env_season = season_layer != nullptr ? static_cast<int>(season_layer->currentSeason()) : -1;
    const float env_season_progress = season_layer != nullptr ? season_layer->seasonProgress() : -1.0f;
    const float env_time_of_day = tod_layer != nullptr ? tod_layer->timeOfDay() : -1.0f;
    const float env_light_level = tod_layer != nullptr ? tod_layer->lightLevel() : -1.0f;
    const int env_weather = weather_layer != nullptr ? static_cast<int>(weather_layer->currentWeather()) : -1;

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
        // Audit § 4.4 / A1.4 trace: peer_signal_heard readback for
        // post-hoc attribution of the chain "heard signal -> turn ->
        // enter halo". recent_signal_dx/dy are world-space offsets to
        // the most recent peer_signal_heard event; salience is the
        // event's stored salience (proxy for proximity at hear time);
        // age_seconds is now - event_time. All four are 0 / -1 when
        // no such event exists.
        float recent_signal_dx = 0.0f;
        float recent_signal_dy = 0.0f;
        float recent_signal_salience = 0.0f;
        float recent_signal_age_seconds = -1.0f;

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

            const auto recent_signal = episodic.recentOfType("peer_signal_heard", 1);
            if (!recent_signal.empty()) {
                const auto& evt = recent_signal.front();
                recent_signal_dx = evt.pos.x - pos.x;
                recent_signal_dy = evt.pos.y - pos.y;
                recent_signal_salience = evt.salience;
                recent_signal_age_seconds = static_cast<float>(world.currentTimeSeconds() - evt.time_seconds);
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
                     << dnc_read_focus_slot << ','
                     << recent_signal_dx << ','
                     << recent_signal_dy << ','
                     << recent_signal_salience << ','
                     << recent_signal_age_seconds;

        // A3.S5 MI inputs. Three env-state columns sourced from world
        // layers (not from perception or memory) so the MI score
        // measures correlation between channel activity and ground
        // truth, not between channel and the listener's belief.
        //   nearest_food_dist  -> ResourceLayer.nearestAvailable distance
        //                         (-1 if no available resource)
        //   predator_nearby_count -> #agents with isPredator() within 12m
        //   worksite_distance  -> distance to nearest ACTIVE worksite
        //                         (-1 if no active worksite)
        float nearest_food_dist = -1.0f;
        if (resources != nullptr) {
            const auto nf = resources->nearestAvailable(pos);
            if (nf.has_value()) {
                nearest_food_dist = nf->distance;
            }
        }
        std::size_t predator_nearby_count = 0;
        constexpr float kPredatorNearbyRadius = 12.0f;
        constexpr float kPredatorNearbyRadiusSq =
            kPredatorNearbyRadius * kPredatorNearbyRadius;
        for (const auto* other : agents->agents()) {
            if (other == nullptr || other == agent || !other->alive()) {
                continue;
            }
            if (!other->isPredator()) {
                continue;
            }
            if (Vec2::distanceSquared(other->position(), pos) <= kPredatorNearbyRadiusSq) {
                ++predator_nearby_count;
            }
        }
        float worksite_distance = -1.0f;
        if (worksites != nullptr) {
            const auto site = worksites->nearestActiveSite(pos);
            if (site.has_value()) {
                worksite_distance = std::sqrt(Vec2::distanceSquared(site->position, pos));
            }
        }
        agent_trace_ << ','
                     << nearest_food_dist << ','
                     << predator_nearby_count << ','
                     << worksite_distance << ','
                     << env_season << ','
                     << env_season_progress << ','
                     << env_time_of_day << ','
                     << env_light_level << ','
                     << env_weather << ','
                     << agent->coreTemperature() << ','
                     << agent->driveState().hunger << '\n';
    }
    agent_trace_.flush();
}

void TraceRecorder::writeWorldSnapshot(World2D& world) {
    if (checkAndCapStream(world_trace_, world_cap_warned_,
                          trace_dir_ / "world_trace.csv",
                          config_.max_bytes_per_file)) {
        return;
    }
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

// L7 R1: emit one JSON line per N ticks for viewer consumption.
// Format: {"tick":T,"t":S,"agents":[{id,a,x,y,e,L}...],"worksites":[{id,bt,p,x,y}...],
//          "trees":[{id,x,y,iv,av}...],"w":weather,"s":season,"wp":weather_progress,"sp":season_progress}
// Field codes are short to keep file size reasonable. Each line is a
// self-contained JSON object (jsonlines format).
// L7 R3 (LAYER7_GODOT_VIEWER_SPEC § 4.1 close-out): trees array + weather
// progress + season progress added so the viewer can render harvest state
// and weather/season transitions instead of just the discrete type.
void TraceRecorder::writeViewerFrame(World2D& world) {
    if (checkAndCapStream(viewer_frames_, viewer_cap_warned_,
                          trace_dir_ / "viewer_frames.jsonl",
                          config_.max_bytes_per_file)) {
        return;
    }
    auto* agents = world.getLayer<AgentLayer>();
    auto* worksites = world.getLayer<WorksiteLayer>();
    auto* trees = world.getLayer<TreeLayer>();
    auto* weather = world.getLayer<WeatherLayer>();
    auto* season = world.getLayer<SeasonLayer>();
    auto* fires = world.getLayer<CampfireLayer>();
    auto* objects = world.getLayer<ObjectLayer>();
    auto* tod = world.getLayer<TimeOfDayLayer>();

    viewer_frames_ << "{\"tick\":" << world.currentTick()
                   << ",\"t\":" << world.currentTimeSeconds();

    viewer_frames_ << ",\"agents\":[";
    bool first = true;
    if (agents != nullptr) {
        for (const auto* a : agents->agents()) {
            if (a == nullptr) continue;
            if (!first) viewer_frames_ << ',';
            first = false;
            viewer_frames_ << "{\"id\":" << a->id().value
                           << ",\"a\":" << static_cast<int>(a->archetypeTag())
                           << ",\"x\":" << a->position().x
                           << ",\"y\":" << a->position().y
                           << ",\"e\":" << a->energy()
                           << ",\"L\":" << static_cast<int>(a->lifecycleStage())
                           << ",\"al\":" << (a->alive() ? 1 : 0)
                           << "}";
        }
    }
    viewer_frames_ << "]";

    viewer_frames_ << ",\"worksites\":[";
    first = true;
    if (worksites != nullptr) {
        for (const auto& site : worksites->sites()) {
            if (!first) viewer_frames_ << ',';
            first = false;
            viewer_frames_ << "{\"id\":" << site.id
                           << ",\"bt\":" << static_cast<int>(site.building_type)
                           << ",\"x\":" << site.position.x
                           << ",\"y\":" << site.position.y
                           << ",\"p\":" << site.build_progress
                           << ",\"a\":" << (site.active ? 1 : 0)
                           << ",\"c\":" << (site.completed ? 1 : 0)
                           << "}";
        }
    }
    viewer_frames_ << "]";

    // L7 R3: trees with integrity ratio so viewer can show harvest state.
    viewer_frames_ << ",\"trees\":[";
    first = true;
    if (trees != nullptr) {
        for (const auto& t : trees->trees()) {
            if (!first) viewer_frames_ << ',';
            first = false;
            const float iv = t.max_integrity > 0.0f ? (t.integrity / t.max_integrity) : 0.0f;
            viewer_frames_ << "{\"id\":" << t.id
                           << ",\"x\":" << t.position.x
                           << ",\"y\":" << t.position.y
                           << ",\"iv\":" << iv
                           << ",\"av\":" << (t.available ? 1 : 0)
                           << "}";
        }
    }
    viewer_frames_ << "]";

    // L7 R4: campfires (fire sources) so the 3D viewer can render flame/glow.
    viewer_frames_ << ",\"fires\":[";
    first = true;
    if (fires != nullptr) {
        for (const auto& fsite : fires->fires()) {
            if (!first) viewer_frames_ << ',';
            first = false;
            viewer_frames_ << "{\"x\":" << fsite.position.x
                           << ",\"y\":" << fsite.position.y
                           << ",\"i\":" << fsite.intensity
                           << ",\"a\":" << (fsite.active ? 1 : 0)
                           << "}";
        }
    }
    viewer_frames_ << "]";

    // L7 R4: movable objects (stones/sticks/spears). Thrown projectiles have
    // non-zero velocity; carried ones ride along an agent. Rendered as small
    // points. t=type, c=carried, vx/vy=velocity (for in-flight trails).
    viewer_frames_ << ",\"objects\":[";
    first = true;
    if (objects != nullptr) {
        for (const auto& o : objects->objects()) {
            if (!first) viewer_frames_ << ',';
            first = false;
            viewer_frames_ << "{\"x\":" << o.pos.x
                           << ",\"y\":" << o.pos.y
                           << ",\"t\":" << static_cast<int>(o.type)
                           << ",\"c\":" << (o.carried ? 1 : 0)
                           << ",\"vx\":" << o.velocity.x
                           << ",\"vy\":" << o.velocity.y
                           << "}";
        }
    }
    viewer_frames_ << "]";

    // L7 R3: weather/season progress (0..1) so viewer can crossfade transitions.
    float wp = 0.0f;
    if (weather != nullptr) {
        const auto planned = weather->plannedDuration();
        if (planned > 0.0) {
            wp = static_cast<float>(weather->timeInState() / planned);
            if (wp > 1.0f) wp = 1.0f;
            if (wp < 0.0f) wp = 0.0f;
        }
    }
    const float sp = season != nullptr ? season->seasonProgress() : 0.0f;

    // L7 R4: day-night phase (0..1, 0.25=sunrise 0.75=sunset) + light level
    // (0..1) so the viewer can drive sun position and ambient lighting.
    const float tod_phase = tod != nullptr ? tod->timeOfDay() : 0.5f;
    const float light = tod != nullptr ? tod->lightLevel() : 1.0f;

    viewer_frames_ << ",\"w\":" << (weather != nullptr ? static_cast<int>(weather->currentWeather()) : 0)
                   << ",\"wp\":" << wp
                   << ",\"s\":" << (season != nullptr ? static_cast<int>(season->currentSeason()) : 0)
                   << ",\"sp\":" << sp
                   << ",\"tod\":" << tod_phase
                   << ",\"light\":" << light
                   << "}\n";
    viewer_frames_.flush();
}

// L7 R4: static world header for the 3D web viewer. Written once when the
// recorder opens (terrain height is constant for an episode; water sources
// and resource cells are seeded at world setup). The renderer builds the
// terrain mesh + scatters ground cover from this, then animates per-frame
// entities from viewer_frames.jsonl on top.
void TraceRecorder::writeViewerWorld(World2D& world, const ProtagonistEcologyConfig& route) {
    std::ofstream vw(trace_dir_ / "viewer_world.json", std::ios::out | std::ios::trunc);
    if (!vw.is_open()) {
        return;
    }
    const float world_w = route.world_size.x;
    const float world_h = route.world_size.y;
    auto* terrain = world.getLayer<TerrainLayer>();
    auto* water = world.getLayer<WaterLayer>();
    auto* resources = world.getLayer<ResourceLayer>();

    // Heightmap sampling grid. 128x128 = 16k samples (one-off), enough for a
    // smooth terrain mesh without bloating the JSON.
    constexpr int kGX = 128;
    constexpr int kGY = 128;

    vw << "{\"world_size\":[" << world_w << "," << world_h << "]"
       << ",\"grid\":[" << kGX << "," << kGY << "]"
       << ",\"height_scale\":" << (terrain != nullptr ? terrain->heightScale() : 0.0f);

    vw << ",\"height\":[";
    if (terrain != nullptr) {
        bool first = true;
        for (int j = 0; j < kGY; ++j) {
            const float y = (static_cast<float>(j) + 0.5f) / kGY * world_h;
            for (int i = 0; i < kGX; ++i) {
                const float x = (static_cast<float>(i) + 0.5f) / kGX * world_w;
                if (!first) vw << ',';
                first = false;
                vw << terrain->heightAt(x, y);
            }
        }
    }
    vw << "]";

    vw << ",\"water\":[";
    if (water != nullptr) {
        bool first = true;
        for (const auto& s : water->sources()) {
            if (!first) vw << ',';
            first = false;
            vw << "{\"x\":" << s.position.x
               << ",\"y\":" << s.position.y
               << ",\"r\":" << s.radius << "}";
        }
    }
    vw << "]";

    // Resource cells: static positions tinted by kind (0=grass 1=browse
    // 2=fruit 3=meat). Drawn as ground scatter so the map reads as a
    // living ecosystem rather than bare terrain.
    vw << ",\"resources\":[";
    if (resources != nullptr) {
        bool first = true;
        for (const auto& r : resources->resources()) {
            if (!first) vw << ',';
            first = false;
            vw << "{\"x\":" << r.position.x
               << ",\"y\":" << r.position.y
               << ",\"k\":" << static_cast<int>(r.kind) << "}";
        }
    }
    vw << "]";

    // Realism (2026-06): cave footprint. Caves are a thermal + predator refuge
    // in the sim (TerrainLayer cave mask) but were never exported, so the world
    // looked cave-less. Sampled once here on a coarse grid; "e":1 marks a cave
    // cell that borders open ground (a mouth) so the viewer can place an arch /
    // entrance there, while interior cells render as dark sheltered hollows.
    vw << ",\"caves\":[";
    if (terrain != nullptr) {
        constexpr int kCX = 110;
        constexpr int kCY = 110;
        const float dx = world_w / static_cast<float>(kCX);
        const float dy = world_h / static_cast<float>(kCY);
        bool first = true;
        for (int j = 0; j < kCY; ++j) {
            const float y = (static_cast<float>(j) + 0.5f) * dy;
            for (int i = 0; i < kCX; ++i) {
                const float x = (static_cast<float>(i) + 0.5f) * dx;
                if (!terrain->inCave(x, y)) continue;
                const bool edge =
                    !terrain->inCave(x - dx, y) || !terrain->inCave(x + dx, y) ||
                    !terrain->inCave(x, y - dy) || !terrain->inCave(x, y + dy);
                if (!first) vw << ',';
                first = false;
                vw << "{\"x\":" << x << ",\"y\":" << y
                   << ",\"e\":" << (edge ? 1 : 0) << "}";
            }
        }
    }
    vw << "]";

    vw << "}\n";
}

}  // namespace neuro::routes::protagonist
