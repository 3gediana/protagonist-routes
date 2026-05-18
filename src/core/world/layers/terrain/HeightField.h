#pragma once

#include <cstddef>
#include <vector>

namespace neuro {

class HeightField {
public:
    HeightField(std::size_t width, std::size_t height, float cell_size);

    float heightAt(float x, float y) const;
    float slopeX(float x, float y) const;
    float slopeY(float x, float y) const;

    float& at(std::size_t x, std::size_t y);
    float at(std::size_t x, std::size_t y) const;

    std::size_t width() const { return width_; }
    std::size_t height() const { return height_; }
    float cellSize() const { return cell_size_; }

    float minHeight() const;
    float maxHeight() const;

private:
    std::size_t width_;
    std::size_t height_;
    float cell_size_;
    std::vector<float> data_;
};

}  // namespace neuro
