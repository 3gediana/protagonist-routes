#include "core/capability/TerrainModifier.h"

#include "core/agent/Agent.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/terrain/TerrainLayer.h"

#include <algorithm>
#include <cmath>

namespace neuro {

TerrainModifier::TerrainModifier(float dig_rate, float build_rate, float energy_cost_per_unit)
    : dig_rate_(dig_rate > 0.0f ? dig_rate : 1.0f),
      build_rate_(build_rate > 0.0f ? build_rate : 1.0f),
      energy_cost_(energy_cost_per_unit > 0.0f ? energy_cost_per_unit : 0.5f) {}

void TerrainModifier::apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) {
    if (outputs.size() < 2) return;

    auto* terrain = world.getLayer<TerrainLayer>();
    if (terrain == nullptr) return;

    float dig = std::clamp(outputs[0], 0.0f, 1.0f);
    float build = std::clamp(outputs[1], 0.0f, 1.0f);

    const float net_change = (build * build_rate_ - dig * dig_rate_) * dt;
    if (std::abs(net_change) < 0.001f) return;

    const float cost = std::abs(net_change) * energy_cost_;
    if (agent.energy() < cost) return;

    auto& hf = terrain->heightFieldMut();
    const float cell_size = hf.cellSize();
    const Vec2 pos = agent.position();

    const std::size_t cx = static_cast<std::size_t>(
        std::clamp(static_cast<int>(pos.x / cell_size), 0, static_cast<int>(hf.width() - 1)));
    const std::size_t cy = static_cast<std::size_t>(
        std::clamp(static_cast<int>(pos.y / cell_size), 0, static_cast<int>(hf.height() - 1)));

    const float weights[3][3] = {
        {0.25f, 0.5f, 0.25f},
        {0.5f,  1.0f, 0.5f},
        {0.25f, 0.5f, 0.25f}
    };

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            const int gx_int = static_cast<int>(cx) + dx;
            const int gy_int = static_cast<int>(cy) + dy;
            if (gx_int < 0 || gy_int < 0) continue;
            const std::size_t gx = static_cast<std::size_t>(gx_int);
            const std::size_t gy = static_cast<std::size_t>(gy_int);
            if (gx >= hf.width() || gy >= hf.height()) continue;

            float& h = hf.at(gx, gy);
            h += net_change * weights[dy + 1][dx + 1];
            h = std::clamp(h, 0.0f, terrain->heightScale() * 1.5f);
        }
    }

    if (dig > 0.5f) {
        auto& cave = terrain->caveMaskMut();
        const float center_h = hf.at(cx, cy);
        if (center_h < terrain->heightScale() * 0.3f) {
            cave.set(cx, cy, true);
        }
    }

    if (build > 0.5f) {
        auto& cave = terrain->caveMaskMut();
        const float center_h = hf.at(cx, cy);
        if (center_h > terrain->heightScale() * 0.5f) {
            cave.set(cx, cy, false);
        }
    }

    agent.setEnergy(agent.energy() - cost);
    world.eventBus().publish(events::Event{events::EventType::TerrainModified,
                                           events::TerrainModified{agent.id(), pos, dig, build},
                                           world.currentTick(),
                                           world.currentTimeSeconds()});
}

}  // namespace neuro
