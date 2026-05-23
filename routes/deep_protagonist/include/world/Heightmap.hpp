#pragma once

#include <cstdint>
#include <vector>

namespace dp::world {

// 2D heightfield: a regular grid of [size_x x size_y] samples, each storing
// the terrain height at that grid cell. Heights are in world meters.
//
// World layout:
//   - x grows east, y grows north (both in [0, size_x*scale))
//   - height is along world Z axis (up)
//   - cell (i,j) stores height at world coordinates (i*scale, j*scale)
class Heightmap {
public:
    Heightmap(int size_x, int size_y, float cell_scale);

    int size_x() const { return size_x_; }
    int size_y() const { return size_y_; }
    float cell_scale() const { return cell_scale_; }
    float world_size_x() const { return (size_x_ - 1) * cell_scale_; }
    float world_size_y() const { return (size_y_ - 1) * cell_scale_; }

    // Direct cell access (clamps to grid bounds)
    float at(int i, int j) const;
    void set(int i, int j, float h);

    // Sample height at world coordinates (bilinear interpolation)
    float sample(float wx, float wy) const;

    // Surface gradient at world coordinates (used for slide-along-slope physics)
    void sample_gradient(float wx, float wy, float& dh_dx, float& dh_dy) const;

    // Generate fractal terrain via simplex-like noise (deterministic, seeded)
    // - max_height: peak elevation in meters
    // - frequency: noise frequency (smaller = bigger features)
    // - octaves:   how many noise layers to stack (more = more detail)
    // - persistence: how fast each octave's amplitude shrinks
    void fill_fractal_noise(uint64_t seed,
                            float max_height = 30.0f,
                            float frequency = 0.005f,
                            int octaves = 5,
                            float persistence = 0.5f);

    const std::vector<float>& raw() const { return data_; }

private:
    int size_x_;
    int size_y_;
    float cell_scale_;
    std::vector<float> data_;  // row-major: data_[i + j * size_x_]

    inline int idx(int i, int j) const { return i + j * size_x_; }
};

}  // namespace dp::world
