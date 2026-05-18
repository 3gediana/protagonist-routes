#pragma once

#include <cstddef>
#include <vector>

namespace neuro {

class CaveMask {
public:
    CaveMask(std::size_t width, std::size_t height, float cell_size);

    bool isCave(float x, float y) const;
    bool justEntered(float from_x, float from_y, float to_x, float to_y) const;
    bool justLeft(float from_x, float from_y, float to_x, float to_y) const;

    void set(std::size_t x, std::size_t y, bool value);
    bool at(std::size_t x, std::size_t y) const;

    std::size_t width() const { return width_; }
    std::size_t height() const { return height_; }
    float cellSize() const { return cell_size_; }

private:
    std::size_t toGridX(float x) const;
    std::size_t toGridY(float y) const;

    std::size_t width_;
    std::size_t height_;
    float cell_size_;
    std::vector<bool> data_;
};

}  // namespace neuro
