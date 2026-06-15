#include "perception/WorldHistoryPerception.h"

#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "world/BaseLayer.h"
#include "world/CampfireLayer.h"
#include "world/WorksiteLayer.h"

#include <algorithm>
#include <cmath>

namespace neuro::routes::protagonist {

void WorldHistoryPerception::sense(const IAgent& self, const IWorld& world,
                                   std::span<float> out) const {
    if (out.size() < kDim) {
        return;
    }
    std::fill(out.begin(), out.begin() + kDim, 0.0f);

    const Vec2 pos = self.position();
    const float r2 = sense_radius_ * sense_radius_;

    // [0] local accumulated-development level: how built-up is the ground around
    // me. Durable structures = campfires + worksites the population has raised.
    float development = 0.0f;
    if (const auto* fires = world.getLayer<CampfireLayer>()) {
        for (const auto& fire : fires->fires()) {
            if (Vec2::distanceSquared(fire.position, pos) <= r2) {
                development += 1.0f;
            }
        }
    }
    if (const auto* worksites = world.getLayer<WorksiteLayer>()) {
        for (const auto& site : worksites->sites()) {
            if (site.build_progress > 0.0f && Vec2::distanceSquared(site.position, pos) <= r2) {
                // Weight by how finished it is (a raised wall develops the land
                // more than a freshly-staked plot).
                development += std::clamp(site.build_progress, 0.0f, 1.0f);
            }
        }
    }
    // Standing inside the colony base is itself a developed location.
    if (const auto* base = world.getLayer<BaseLayer>()) {
        if (base->inBase(pos)) {
            development += 1.0f;
        }
    }
    constexpr float kDevelopmentCap = 4.0f;  // ~4 nearby structures = fully developed
    out[0] = std::clamp(development / kDevelopmentCap, 0.0f, 1.0f);

    // [1] world age signal: saturating map of the world clock so the value stays
    // in [0,1) and rises monotonically as the run ages.
    const double t = static_cast<double>(world.currentTimeSeconds());
    constexpr double kAgeTau = 600.0;  // seconds; ~half-saturation around 7 min
    out[1] = static_cast<float>(1.0 - std::exp(-std::max(0.0, t) / kAgeTau));
}

}  // namespace neuro::routes::protagonist
