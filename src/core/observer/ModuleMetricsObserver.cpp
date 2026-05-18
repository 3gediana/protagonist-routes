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
         << "avg_energy,total_damage_dealt,total_kills,"
         << "max_infected,infection_death_ratio,"
         << "avg_food_eaten,resource_available_count,avg_resource_amount,"
         << "min_light,max_light,min_season,max_season,"
         << "signal_field_energy,heavy_resources_available,heavy_workers_avg,"
         << "avg_complexity\n";
}

void ModuleMetricsObserver::onAttach(IWorld& world) {
    (void)world;
    max_infected_ = 0;
    min_light_ = 2.0f;
    max_light_ = -1.0f;
    min_season_ = 999;
    max_season_ = 0;
}

void ModuleMetricsObserver::onTick(IWorld& world) {
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
    auto* agent_layer = world.getLayer<AgentLayer>();
    if (agent_layer) {
        std::size_t infected = 0;
        for (const auto* agent : agent_layer->agents()) {
            if (!agent->alive()) continue;
            const auto* concrete = dynamic_cast<const Agent*>(agent);
            if (concrete && concrete->infected()) ++infected;
        }
        if (infected > max_infected_) max_infected_ = infected;
    }
}

void ModuleMetricsObserver::onDetach(IWorld& world) {
    auto* agent_layer = world.getLayer<AgentLayer>();
    if (agent_layer == nullptr) return;

    const auto& agents = agent_layer->agents();

    std::size_t alive = 0;
    std::size_t predator_count = 0;
    std::size_t prey_count = 0;
    double energy_sum = 0.0;
    double damage_sum = 0.0;
    std::size_t kill_sum = 0;
    std::size_t infected_count = 0;
    double food_eaten_sum = 0.0;

    for (const auto* agent : agents) {
        if (!agent->alive()) continue;
        ++alive;
        energy_sum += agent->energy();

        const auto* concrete = dynamic_cast<const Agent*>(agent);
        if (concrete) {
            if (concrete->isPredator()) {
                ++predator_count;
            } else {
                ++prey_count;
            }
            damage_sum += concrete->damageDealt();
            kill_sum += concrete->kills();
            if (concrete->infected()) {
                ++infected_count;
            }
            food_eaten_sum += concrete->foodEatenThisTick();
            if (concrete->damageDealt() > 0 || concrete->kills() > 0) {
                // track cumulative
            }
        }
    }

    const double avg_energy = alive > 0 ? energy_sum / alive : 0.0;
    const double avg_food = alive > 0 ? food_eaten_sum / alive : 0.0;

    // Disease metrics
    std::size_t infection_deaths = 0;
    const auto* disease = world.getLayer<DiseaseLayer>();
    if (disease) {
        infection_deaths = disease->infectionDeathCount();
    }
    const double infection_death_ratio = alive > 0
        ? static_cast<double>(max_infected_) / alive : 0.0;

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

    // Day/Night
    float light_level = 1.0f;
    const auto* tod = world.getLayer<TimeOfDayLayer>();
    if (tod) {
        light_level = tod->lightLevel();
    }

    // Season
    std::size_t season_index = 0;
    float season_progress = 0.0f;
    const auto* season = world.getLayer<SeasonLayer>();
    if (season) {
        season_index = season->seasonIndex();
        season_progress = season->seasonProgress();
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
         << avg_energy << ','
         << damage_sum << ','
         << kill_sum << ','
         << max_infected_ << ','
         << infection_death_ratio << ','
         << avg_food << ','
         << resource_available << ','
         << avg_resource_amount << ','
         << min_light_ << ','
         << max_light_ << ','
         << min_season_ << ','
         << max_season_ << ','
         << signal_energy << ','
         << heavy_available << ','
         << heavy_workers_avg << ','
         << 0.0 << '\n';
    csv_.flush();

    ++generation_;
}

const char* ModuleMetricsObserver::name() const { return kName; }

}  // namespace neuro
