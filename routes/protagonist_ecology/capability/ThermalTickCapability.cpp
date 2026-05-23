#include "capability/ThermalTickCapability.h"

#include "core/agent/Agent.h"
#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/SeasonLayer.h"
#include "core/world/layers/TimeOfDayLayer.h"
#include "world/CampfireLayer.h"

#include <algorithm>
#include <cmath>

namespace neuro::routes::protagonist {

void ThermalTickCapability::apply(IAgent& agent,
                                  IWorld& world,
                                  std::span<const float> /*outputs*/,
                                  float dt) {
    auto* concrete = dynamic_cast<Agent*>(&agent);
    if (concrete == nullptr) return;
    if (dt <= 0.0f) return;

    // Step 1: compute ambient temperature.
    float ambient = coeffs_.ambient_spring;  // fallback
    if (const auto* season = world.getLayer<SeasonLayer>(); season != nullptr) {
        switch (season->currentSeason()) {
            case SeasonType::Spring:  ambient = coeffs_.ambient_spring;  break;
            case SeasonType::Summer:  ambient = coeffs_.ambient_summer;  break;
            case SeasonType::Autumn:  ambient = coeffs_.ambient_autumn;  break;
            case SeasonType::Winter:  ambient = coeffs_.ambient_winter;  break;
        }
    }
    if (const auto* tod = world.getLayer<TimeOfDayLayer>(); tod != nullptr) {
        ambient += tod->ambientTemperatureDelta();
    }

    // Step 2: compute heat in (metabolic + fire warming).
    const auto v = concrete->velocity();
    const float speed = std::sqrt(v.x * v.x + v.y * v.y);
    float heat_in_per_sec = coeffs_.metabolic_heat_per_speed_per_sec * speed;

    if (const auto* fire = world.getLayer<CampfireLayer>(); fire != nullptr) {
        if (fire->inWarmthZone(concrete->position())) {
            heat_in_per_sec += coeffs_.fire_warming_per_sec;
        }
    }

    // Step 3: integrate.
    //   dT/dt = heat_in_per_sec - k × (T - ambient)
    const float t = concrete->coreTemperature();
    const float dt_per_sec = heat_in_per_sec - coeffs_.radiative_loss_per_sec * (t - ambient);
    float new_t = t + dt_per_sec * dt;
    new_t = std::clamp(new_t,
                       coeffs_.min_core_temperature,
                       coeffs_.max_core_temperature);
    concrete->setCoreTemperature(new_t);
}

}  // namespace neuro::routes::protagonist
