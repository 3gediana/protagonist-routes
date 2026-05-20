#include "core/reward/HabitatReward.h"

#include "core/agent/Agent.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/terrain/TerrainLayer.h"
#include "core/world/layers/TimeOfDayLayer.h"

#include <algorithm>
#include <cmath>

namespace neuro {

HabitatReward::HabitatReward(double weight, float height_scale)
    : weight_(weight), height_scale_(height_scale > 0 ? height_scale : 50.0f) {}

double HabitatReward::evaluate(const IAgent& agent, const IWorld& world, float dt) const {
    const auto* terrain = world.getLayer<TerrainLayer>();
    if (!terrain) return 0.0;

    const auto* concrete = dynamic_cast<const Agent*>(&agent);
    if (!concrete) return 0.0;

    const float h = terrain->heightAt(agent.position().x, agent.position().y);
    const float effective_height_scale = terrain->heightScale() > 0.0f ? terrain->heightScale() : height_scale_;
    const float h_norm = std::clamp(h / effective_height_scale, 0.0f, 1.0f);
    const float pref = concrete->habitatPreference();

    // match_score: 1 when h_norm == pref, 0 when maximally mismatched
    const float match = 1.0f - std::abs(h_norm - pref);
    return static_cast<double>(match) * dt;
}

ActivityReward::ActivityReward(double weight) : weight_(weight) {}

double ActivityReward::evaluate(const IAgent& agent, const IWorld& world, float dt) const {
    const auto* tod = world.getLayer<TimeOfDayLayer>();
    if (!tod) return 0.0;

    const auto* concrete = dynamic_cast<const Agent*>(&agent);
    if (!concrete) return 0.0;

    const float light = tod->lightLevel();  // 0=night, 1=day
    const float pref = concrete->activityPeriod();  // 0=nocturnal, 1=diurnal

    // match: high when light level matches activity preference
    const float match = 1.0f - std::abs(light - pref);
    return static_cast<double>(match) * dt;
}

}  // namespace neuro
