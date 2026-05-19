#include "core/world/layers/terrain/CaveMask.h"

#include <algorithm>
#include <cmath>

namespace neuro {

CaveMask::CaveMask(std::size_t width, std::size_t height, float cell_size)
    : width_(width),
      height_(height),
      cell_size_(cell_size > 0.0f ? cell_size : 1.0f),
      data_(width * height, false) {}

bool CaveMask::isCave(float x, float y) const {
    const std::size_t gx = toGridX(x);
    const std::size_t gy = toGridY(y);
    return at(gx, gy);
}

bool CaveMask::justEntered(float from_x, float from_y, float to_x, float to_y) const {
    return !isCave(from_x, from_y) && isCave(to_x, to_y);
}

bool CaveMask::justLeft(float from_x, float from_y, float to_x, float to_y) const {
    return isCave(from_x, from_y) && !isCave(to_x, to_y);
}

void CaveMask::set(std::size_t x, std::size_t y, bool value) {
    data_[y * width_ + x] = value;
}

bool CaveMask::at(std::size_t x, std::size_t y) const {
    if (x >= width_ || y >= height_) return false;
    return data_[y * width_ + x];
}

std::size_t CaveMask::toGridX(float x) const {
    if (!std::isfinite(x)) return 0;
    const std::size_t gx = static_cast<std::size_t>(std::max(0.0f, std::floor(x / cell_size_)));
    return std::min(gx, width_ - 1);
}

std::size_t CaveMask::toGridY(float y) const {
    if (!std::isfinite(y)) return 0;
    const std::size_t gy = static_cast<std::size_t>(std::max(0.0f, std::floor(y / cell_size_)));
    return std::min(gy, height_ - 1);
}

}  // namespace neuro
