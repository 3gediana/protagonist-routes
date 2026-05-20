#include "core/world/layers/signal/SignalLayer.h"
#include "core/interfaces/IWorld.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace neuro {

SignalLayer::SignalLayer(Vec2 world_size,
                         std::size_t grid_resolution,
                         std::size_t channels,
                         float decay_rate,
                         float diffusion_rate)
    : world_size_(world_size),
      grid_res_(grid_resolution > 0 ? grid_resolution : 32),
      channels_(channels > 0 ? channels : kDefaultChannels),
      decay_rate_(decay_rate),
      diffusion_rate_(diffusion_rate),
      field_(grid_res_ * grid_res_, std::vector<float>(channels_, 0.0f)) {}

void SignalLayer::onAttach(IWorld& world) {
    (void)world;
    for (auto& cell : field_) {
        std::fill(cell.begin(), cell.end(), 0.0f);
    }
}

void SignalLayer::tick(IWorld& world, float dt) {
    (void)world;
    applyDecay(dt);
    applyDiffusion();
}

void SignalLayer::emit(Vec2 position, const std::vector<float>& signal) {
    const std::size_t idx = gridIndex(position.x, position.y);
    if (idx >= field_.size()) return;
    auto& cell = field_[idx];
    for (std::size_t c = 0; c < channels_ && c < signal.size(); ++c) {
        cell[c] += signal[c];
    }
}

void SignalLayer::emitWithHistory(AgentId source, Vec2 position, const std::vector<float>& signal, std::size_t tick) {
    emit(position, signal);
    for (std::size_t c = 0; c < channels_ && c < signal.size(); ++c) {
        if (signal[c] > 0.01f) {
            history_.record(source, c, position, tick, signal[c]);
        }
    }
}

std::vector<float> SignalLayer::sample(Vec2 position, float radius) const {
    std::vector<float> result(channels_, 0.0f);

    const float cell_size = world_size_.x / static_cast<float>(grid_res_);
    const float radius_cells = radius / cell_size;

    const float cx = position.x / cell_size;
    const float cy = position.y / cell_size;

    const std::size_t min_gx = static_cast<std::size_t>(std::max(0.0f, cx - radius_cells));
    const std::size_t max_gx = static_cast<std::size_t>(std::min(static_cast<float>(grid_res_ - 1), cx + radius_cells));
    const std::size_t min_gy = static_cast<std::size_t>(std::max(0.0f, cy - radius_cells));
    const std::size_t max_gy = static_cast<std::size_t>(std::min(static_cast<float>(grid_res_ - 1), cy + radius_cells));

    float total_weight = 0.0f;

    for (std::size_t gy = min_gy; gy <= max_gy; ++gy) {
        for (std::size_t gx = min_gx; gx <= max_gx; ++gx) {
            const float cell_center_x = (static_cast<float>(gx) + 0.5f) * cell_size;
            const float cell_center_y = (static_cast<float>(gy) + 0.5f) * cell_size;
            const float dx = cell_center_x - position.x;
            const float dy = cell_center_y - position.y;
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance > radius) {
                continue;
            }

            const float weight = std::max(0.0f, 1.0f - distance / std::max(radius, 1.0e-3f));
            if (weight <= 0.0f) {
                continue;
            }

            const auto& cell = field_[gy * grid_res_ + gx];
            for (std::size_t c = 0; c < channels_; ++c) {
                result[c] += cell[c] * weight;
            }
            total_weight += weight;
        }
    }

    if (total_weight > 0.0f) {
        for (auto& v : result) {
            v /= total_weight;
        }
    }

    return result;
}

void SignalLayer::applyDecay(float dt) {
    const float factor = std::exp(-decay_rate_ * dt);
    for (auto& cell : field_) {
        for (auto& v : cell) {
            v *= factor;
        }
    }
}

void SignalLayer::applyDiffusion() {
    if (diffusion_rate_ <= 0.0f) return;

    std::vector<std::vector<float>> next = field_;

    for (std::size_t gy = 0; gy < grid_res_; ++gy) {
        for (std::size_t gx = 0; gx < grid_res_; ++gx) {
            const std::size_t idx = gy * grid_res_ + gx;
            for (std::size_t c = 0; c < channels_; ++c) {
                float sum = 0.0f;
                int count = 0;

                if (gx > 0) { sum += field_[idx - 1][c]; count++; }
                if (gx + 1 < grid_res_) { sum += field_[idx + 1][c]; count++; }
                if (gy > 0) { sum += field_[(gy - 1) * grid_res_ + gx][c]; count++; }
                if (gy + 1 < grid_res_) { sum += field_[(gy + 1) * grid_res_ + gx][c]; count++; }

                if (count > 0) {
                    next[idx][c] += diffusion_rate_ * (sum / static_cast<float>(count) - field_[idx][c]);
                }
            }
        }
    }

    field_ = std::move(next);
}

std::size_t SignalLayer::gridIndex(float x, float y) const {
    const float cell_size = world_size_.x / static_cast<float>(grid_res_);
    const std::size_t gx = static_cast<std::size_t>(std::clamp(x / cell_size, 0.0f, static_cast<float>(grid_res_ - 1)));
    const std::size_t gy = static_cast<std::size_t>(std::clamp(y / cell_size, 0.0f, static_cast<float>(grid_res_ - 1)));
    return gy * grid_res_ + gx;
}

double SignalLayer::totalEnergy() const {
    double sum = 0.0;
    for (const auto& cell : field_) {
        for (const auto v : cell) {
            sum += static_cast<double>(v) * static_cast<double>(v);
        }
    }
    return sum;
}

}  // namespace neuro
