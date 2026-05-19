#include "core/observer/HeatmapObserver.h"

#include "core/agent/Agent.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IWorld.h"
#include "core/world/layers/AgentLayer.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace neuro {

HeatmapObserver::HeatmapObserver(std::size_t grid_resolution,
                                 std::size_t sample_interval,
                                 const std::filesystem::path& output_dir)
    : grid_resolution_(grid_resolution),
      sample_interval_(sample_interval),
      output_dir_(output_dir),
      heatmap_(grid_resolution * grid_resolution, 0.0f),
      cumulative_(grid_resolution * grid_resolution, 0.0f),
      world_(nullptr),
      tick_sub_id_(0),
      gen_sub_id_(0),
      tick_counter_(0) {}

HeatmapObserver::~HeatmapObserver() = default;

void HeatmapObserver::onAttach(IWorld& world) {
    world_ = &world;
    std::filesystem::create_directories(output_dir_);

    // Reset heatmaps
    std::fill(heatmap_.begin(), heatmap_.end(), 0.0f);
    std::fill(cumulative_.begin(), cumulative_.end(), 0.0f);
    tick_counter_ = 0;

    // Subscribe to tick events for position sampling
    tick_sub_id_ = world.eventBus().subscribe(events::EventType::TickEnded,
        [this](const events::Event&) {
            onTick();
        });

    // Subscribe to generation end for writing CSV
    gen_sub_id_ = world.eventBus().subscribe(events::EventType::GenerationEnded,
        [this](const events::Event& event) {
            const auto& payload = event.payloadAs<events::GenerationEnded>();
            onGenerationEnded(payload.gen);
        });
}

void HeatmapObserver::onDetach(IWorld& world) {
    if (tick_sub_id_ != 0) {
        world.eventBus().unsubscribe(tick_sub_id_);
        tick_sub_id_ = 0;
    }
    if (gen_sub_id_ != 0) {
        world.eventBus().unsubscribe(gen_sub_id_);
        gen_sub_id_ = 0;
    }
    world_ = nullptr;
}

const char* HeatmapObserver::name() const {
    return "heatmap";
}

void HeatmapObserver::onTick() {
    tick_counter_++;
    if (tick_counter_ % sample_interval_ != 0) return;

    if (world_ == nullptr) return;

    const auto* agent_layer = world_->getLayer<AgentLayer>();
    if (agent_layer == nullptr) return;

    const Vec2 world_size = world_->size();

    for (const Agent* agent : agent_layer->agents()) {
        if (!agent->alive()) continue;

        const Vec2 pos = agent->position();
        const std::size_t gx = static_cast<std::size_t>(
            std::clamp(pos.x / world_size.x, 0.0f, 1.0f) * static_cast<float>(grid_resolution_ - 1));
        const std::size_t gy = static_cast<std::size_t>(
            std::clamp(pos.y / world_size.y, 0.0f, 1.0f) * static_cast<float>(grid_resolution_ - 1));

        const std::size_t idx = gy * grid_resolution_ + gx;
        if (idx < heatmap_.size()) {
            heatmap_[idx] += 1.0f;
            cumulative_[idx] += 1.0f;
        }
    }
}

void HeatmapObserver::onGenerationEnded(Generation generation) {
    writeHeatmapCsv(generation);

    // Decay current heatmap for next generation (keep 20% as memory)
    for (auto& val : heatmap_) {
        val *= 0.2f;
    }
}

void HeatmapObserver::writeHeatmapCsv(Generation generation) {
    // Write every 10 generations to save disk space
    if (generation % 10 != 0 && generation != 0) return;

    const auto filename = output_dir_ / ("heatmap_gen" + std::to_string(generation) + ".csv");
    std::ofstream out(filename, std::ios::out | std::ios::trunc);
    if (!out.is_open()) return;

    // Find max value for normalization
    float max_val = 0.0f;
    for (const auto v : cumulative_) {
        max_val = std::max(max_val, v);
    }
    if (max_val < 1.0f) max_val = 1.0f;

    // Write as CSV: row per Y, columns per X
    for (std::size_t y = 0; y < grid_resolution_; ++y) {
        for (std::size_t x = 0; x < grid_resolution_; ++x) {
            if (x > 0) out << ',';
            const float normalized = cumulative_[y * grid_resolution_ + x] / max_val;
            out << normalized;
        }
        out << '\n';
    }
    out.close();
}

}  // namespace neuro
