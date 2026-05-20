#include "core/observer/ModuleMetricsObserver.h"

#include "core/agent/Agent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"
#include "core/world/layers/ResourceLayer.h"
#include "core/world/layers/DiseaseLayer.h"
#include "core/world/layers/SeasonLayer.h"
#include "core/world/layers/TimeOfDayLayer.h"
#include "core/world/layers/HeavyResourceLayer.h"
#include "core/world/layers/signal/SignalLayer.h"

#include <cmath>
#include <numeric>

namespace neuro {

ModuleMetricsObserver::ModuleMetricsObserver(const std::filesystem::path& csv_path)
    : csv_(csv_path, std::ios::out | std::ios::trunc) {
    csv_ << "generation,alive_count,predator_count,prey_count,"
         << "avg_energy,avg_speed,avg_infected_speed,"
         << "total_damage_dealt,total_kills,predator_kills,"
         << "max_infected,infection_deaths,recoveries,immune_count,healthy_infected_dist,"
         << "avg_food_eaten,resource_available_count,avg_resource_amount,"
         << "min_light,max_light,min_season,max_season,current_season,"
         << "signal_field_energy,heavy_resources_available,heavy_workers_avg,"
         << "avg_complexity,"
         << "pathogen_avg_speed,pathogen_avg_spread,pathogen_avg_evasion,pathogen_avg_gen\n";
}

void ModuleMetricsObserver::onAttach(IWorld& world) {
    (void)world;
    resetAccumulators();
}

void ModuleMetricsObserver::resetAccumulators() {
    max_infected_ = 0;
    min_light_ = 2.0f;
    max_light_ = -1.0f;
    min_season_ = 999;
    max_season_ = 0;
    speed_sum_ = 0.0;
    infected_speed_sum_ = 0.0;
    infected_speed_ticks_ = 0;
    healthy_infected_dist_sum_ = 0.0;
    healthy_dist_ticks_ = 0;
    food_eaten_sum_ = 0.0;
    food_agent_samples_ = 0;
    tick_count_ = 0;
    pathogen_speed_sum_ = 0.0;
    pathogen_spread_sum_ = 0.0;
    pathogen_evasion_sum_ = 0.0;
    pathogen_ticks_ = 0;
    pathogen_generation_sum_ = 0;
}

void ModuleMetricsObserver::onTick(IWorld& world) {
    ++tick_count_;

    const auto* tod = world.getLayer<TimeOfDayLayer>();
    if (tod) {
        const float l = tod->lightLevel();
        if (l < min_light_) min_light_ = l;
        if (l > max_light_) max_light_ = l;
    }
    const auto* season = world.getLayer<SeasonLayer>();
    if (season) {
        const auto si = season->seasonIndex();
        if (si < min_season_) min_season_ = si;
        if (si > max_season_) max_season_ = si;
    }

    // Sample behavioral metrics every 10 ticks to reduce overhead
    if (tick_count_ % 10 != 0) return;

    auto* agent_layer = world.getLayer<AgentLayer>();
    if (agent_layer == nullptr) return;

    const auto& agents = agent_layer->agents();
    std::size_t infected_now = 0;

    struct TickInfo { Vec2 pos; bool infected; bool predator; };
    std::vector<TickInfo> infos;
    infos.reserve(agents.size());

    for (const auto* agent : agents) {
        if (!agent->alive()) continue;
        const float speed_raw = agent->velocity().length();
        const float speed = std::isfinite(speed_raw) ? speed_raw : 0.0f;
        speed_sum_ += speed;

        const auto* concrete = dynamic_cast<const Agent*>(agent);
        bool is_inf = concrete ? concrete->infected() : false;
        bool is_pred = concrete ? concrete->isPredator() : false;

        if (is_inf) {
            ++infected_now;
            infected_speed_sum_ += speed;
            ++infected_speed_ticks_;
            // Pathogen metrics
            if (concrete && concrete->pathogen()) {
                pathogen_speed_sum_ += concrete->pathogen()->infectionSpeed();
                pathogen_spread_sum_ += concrete->pathogen()->spreadRate();
                pathogen_evasion_sum_ += concrete->pathogen()->immuneEvasion();
                pathogen_generation_sum_ += concrete->pathogen()->genome().generation();
                ++pathogen_ticks_;
            }
        }

        if (concrete != nullptr) {
            food_eaten_sum_ += concrete->foodEatenThisTick();
            ++food_agent_samples_;
        }

        infos.push_back({agent->position(), is_inf, is_pred});
    }

    if (infected_now > max_infected_) max_infected_ = infected_now;

    for (const auto& info : infos) {
        if (info.infected || info.predator) continue;
        float min_dist = 999.0f;
        for (const auto& other : infos) {
            if (!other.infected) continue;
            const float dx = info.pos.x - other.pos.x;
            const float dy = info.pos.y - other.pos.y;
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < min_dist) min_dist = dist;
        }
        if (min_dist < 999.0f) {
            healthy_infected_dist_sum_ += min_dist;
            ++healthy_dist_ticks_;
        }
    }
}

void ModuleMetricsObserver::onDetach(IWorld& world) {
    auto* agent_layer = world.getLayer<AgentLayer>();
    if (agent_layer == nullptr) return;

    const auto& agents = agent_layer->agents();

    std::size_t alive = 0;
    std::size_t predator_count = 0;
    std::size_t prey_count = 0;
    std::size_t immune_count = 0;
    double energy_sum = 0.0;
    double damage_sum = 0.0;
    std::size_t kill_sum = 0;
    std::size_t predator_kill_sum = 0;

    for (const auto* agent : agents) {
        if (!agent->alive()) continue;
        ++alive;
        energy_sum += agent->energy();

        const auto* concrete = dynamic_cast<const Agent*>(agent);
        if (concrete) {
            if (concrete->isPredator()) {
                ++predator_count;
                predator_kill_sum += concrete->kills();
            } else {
                ++prey_count;
            }
            damage_sum += concrete->damageDealt();
            kill_sum += concrete->kills();
            if (!concrete->immuneMemory().empty()) ++immune_count;
        }
    }

    const double avg_energy = alive > 0 ? energy_sum / alive : 0.0;
    const double avg_food = food_agent_samples_ > 0 ? food_eaten_sum_ / food_agent_samples_ : 0.0;

    // Use tick-accumulated behavioral metrics
    // speed_sum_ accumulates per-agent speed each sample tick
    // Divide by total sample ticks to get average speed across all agents per sample
    const std::size_t sample_ticks = tick_count_ / 10;
    const double avg_speed = sample_ticks > 0 && alive > 0
        ? speed_sum_ / (sample_ticks * alive) : 0.0;
    const double avg_infected_speed = infected_speed_ticks_ > 0
        ? infected_speed_sum_ / infected_speed_ticks_ : 0.0;
    const double avg_healthy_infected_dist = healthy_dist_ticks_ > 0
        ? healthy_infected_dist_sum_ / healthy_dist_ticks_ : 0.0;

    // Sanitize NaN
    auto safe = [](double v) { return std::isfinite(v) ? v : 0.0; };

    // Disease deaths and recoveries
    std::size_t infection_deaths = 0;
    std::size_t recoveries = 0;
    const auto* disease = world.getLayer<DiseaseLayer>();
    if (disease) {
        infection_deaths = disease->infectionDeathCount();
        recoveries = disease->recoveryCount();
    }

    // Resource metrics
    std::size_t resource_available = 0;
    double resource_amount_sum = 0.0;
    const auto* resource_layer = world.getLayer<ResourceLayer>();
    if (resource_layer) {
        for (const auto& r : resource_layer->resources()) {
            if (r.available) {
                ++resource_available;
                resource_amount_sum += r.amount;
            }
        }
    }
    const double avg_resource_amount = resource_available > 0
        ? resource_amount_sum / resource_available : 0.0;

    // Season
    std::size_t current_season = 0;
    const auto* season = world.getLayer<SeasonLayer>();
    if (season) {
        current_season = season->seasonIndex();
    }

    // Signal field energy
    double signal_energy = 0.0;
    const auto* signal = world.getLayer<SignalLayer>();
    if (signal) {
        signal_energy = signal->totalEnergy();
    }

    // Heavy resource metrics
    std::size_t heavy_available = 0;
    double heavy_workers_avg = 0.0;
    const auto* heavy = world.getLayer<HeavyResourceLayer>();
    if (heavy) {
        for (const auto& hr : heavy->resources()) {
            if (hr.available) {
                ++heavy_available;
                heavy_workers_avg += static_cast<double>(hr.current_workers);
            }
        }
        if (heavy_available > 0) heavy_workers_avg /= heavy_available;
    }

    csv_ << generation_ << ','
         << alive << ','
         << predator_count << ','
         << prey_count << ','
         << safe(avg_energy) << ','
         << safe(avg_speed) << ','
         << safe(avg_infected_speed) << ','
         << safe(damage_sum) << ','
         << kill_sum << ','
         << predator_kill_sum << ','
         << max_infected_ << ','
         << infection_deaths << ','
         << recoveries << ','
         << immune_count << ','
         << safe(avg_healthy_infected_dist) << ','
         << safe(avg_food) << ','
         << resource_available << ','
         << safe(avg_resource_amount) << ','
         << min_light_ << ','
         << max_light_ << ','
         << min_season_ << ','
         << max_season_ << ','
         << current_season << ','
         << safe(signal_energy) << ','
         << heavy_available << ','
         << safe(heavy_workers_avg) << ','
         << 0.0 << ','
         << safe(pathogen_ticks_ > 0 ? pathogen_speed_sum_ / pathogen_ticks_ : 0.0) << ','
         << safe(pathogen_ticks_ > 0 ? pathogen_spread_sum_ / pathogen_ticks_ : 0.0) << ','
         << safe(pathogen_ticks_ > 0 ? pathogen_evasion_sum_ / pathogen_ticks_ : 0.0) << ','
         << (pathogen_ticks_ > 0 ? pathogen_generation_sum_ / pathogen_ticks_ : 0.0) << '\n';
    csv_.flush();

    ++generation_;
    resetAccumulators();
}

const char* ModuleMetricsObserver::name() const { return kName; }

}  // namespace neuro
