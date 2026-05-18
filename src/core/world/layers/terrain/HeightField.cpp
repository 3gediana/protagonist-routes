#include "core/world/layers/terrain/HeightField.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace neuro {

HeightField::HeightField(std::size_t width, std::size_t height, float cell_size)
    : width_(width),
      height_(height),
      cell_size_(cell_size > 0.0f ? cell_size : 1.0f),
      data_(width * height, 0.0f) {}

float HeightField::heightAt(float x, float y) const {
    const float gx = x / cell_size_;
    const float gy = y / cell_size_;

    const std::size_t x0 = static_cast<std::size_t>(std::max(0.0f, std::floor(gx)));
    const std::size_t y0 = static_cast<std::size_t>(std::max(0.0f, std::floor(gy)));
    const std::size_t x1 = std::min(x0 + 1, width_ - 1);
    const std::size_t y1 = std::min(y0 + 1, height_ - 1);

    const float fx = std::clamp(gx - static_cast<float>(x0), 0.0f, 1.0f);
    const float fy = std::clamp(gy - static_cast<float>(y0), 0.0f, 1.0f);

    const float h00 = at(x0, y0);
    const float h10 = at(x1, y0);
    const float h01 = at(x0, y1);
    const float h11 = at(x1, y1);

    const float h0 = h00 * (1.0f - fx) + h10 * fx;
    const float h1 = h01 * (1.0f - fx) + h11 * fx;

    return h0 * (1.0f - fy) + h1 * fy;
}

float HeightField::slopeX(float x, float y) const {
    const float delta = cell_size_ * 0.5f;
    const float h_plus = heightAt(x + delta, y);
    const float h_minus = heightAt(x - delta, y);
    return (h_plus - h_minus) / (2.0f * delta);
}

float HeightField::slopeY(float x, float y) const {
    const float delta = cell_size_ * 0.5f;
    const float h_plus = heightAt(x, y + delta);
    const float h_minus = heightAt(x, y - delta);
    return (h_plus - h_minus) / (2.0f * delta);
}

float& HeightField::at(std::size_t x, std::size_t y) {
    return data_[y * width_ + x];
}

float HeightField::at(std::size_t x, std::size_t y) const {
    return data_[y * width_ + x];
}

float HeightField::minHeight() const {
    float min_h = std::numeric_limits<float>::max();
    for (const auto h : data_) {
        min_h = std::min(min_h, h);
    }
    return min_h;
}

float HeightField::maxHeight() const {
    float max_h = std::numeric_limits<float>::lowest();
    for (const auto h : data_) {
        max_h = std::max(max_h, h);
    }
    return max_h;
}

}  // namespace neuro
