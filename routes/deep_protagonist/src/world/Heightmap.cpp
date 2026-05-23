#include "world/Heightmap.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace dp::world {

namespace {

// ---------------------------------------------------------------------------
// Lightweight Perlin-like 2D noise. Self-contained: avoids pulling in a
// dependency just for terrain generation. Quality is fine for a game world.
// ---------------------------------------------------------------------------

// Fast hash producing a stable pseudo-random number in [0,1) from (i,j,seed)
inline float hash2(int i, int j, uint64_t seed) {
    uint64_t x = static_cast<uint64_t>(i) * 0x9E3779B97F4A7C15ULL
               ^ static_cast<uint64_t>(j) * 0xC2B2AE3D27D4EB4FULL
               ^ seed * 0x165667B19E3779F9ULL;
    x ^= x >> 27;
    x *= 0x94D049BB133111EBULL;
    x ^= x >> 31;
    return static_cast<float>(x & 0xFFFFFFu) / 16777216.0f;  // 24-bit mantissa
}

inline float fade(float t) {
    // smoothstep^2: 6t^5 - 15t^4 + 10t^3 (Perlin's classic curve)
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// Value-noise sample at floating-point (x,y) given seed.
// (Faster than Perlin gradient noise; good enough for terrain.)
float value_noise(float x, float y, uint64_t seed) {
    int xi = static_cast<int>(std::floor(x));
    int yi = static_cast<int>(std::floor(y));
    float xf = x - xi;
    float yf = y - yi;
    float u = fade(xf);
    float v = fade(yf);

    float v00 = hash2(xi,     yi,     seed);
    float v10 = hash2(xi + 1, yi,     seed);
    float v01 = hash2(xi,     yi + 1, seed);
    float v11 = hash2(xi + 1, yi + 1, seed);

    return lerp(lerp(v00, v10, u), lerp(v01, v11, u), v);
}

}  // namespace

// ---------------------------------------------------------------------------
// Heightmap
// ---------------------------------------------------------------------------

Heightmap::Heightmap(int size_x, int size_y, float cell_scale)
    : size_x_(size_x), size_y_(size_y), cell_scale_(cell_scale),
      data_(static_cast<size_t>(size_x) * static_cast<size_t>(size_y), 0.0f) {}

float Heightmap::at(int i, int j) const {
    i = std::clamp(i, 0, size_x_ - 1);
    j = std::clamp(j, 0, size_y_ - 1);
    return data_[idx(i, j)];
}

void Heightmap::set(int i, int j, float h) {
    if (i < 0 || i >= size_x_ || j < 0 || j >= size_y_) return;
    data_[idx(i, j)] = h;
}

float Heightmap::sample(float wx, float wy) const {
    float fx = wx / cell_scale_;
    float fy = wy / cell_scale_;
    int i0 = static_cast<int>(std::floor(fx));
    int j0 = static_cast<int>(std::floor(fy));
    float u = fx - i0;
    float v = fy - j0;

    float h00 = at(i0,     j0);
    float h10 = at(i0 + 1, j0);
    float h01 = at(i0,     j0 + 1);
    float h11 = at(i0 + 1, j0 + 1);

    return (1 - u) * (1 - v) * h00 + u * (1 - v) * h10
         + (1 - u) * v       * h01 + u * v       * h11;
}

void Heightmap::sample_gradient(float wx, float wy, float& dh_dx, float& dh_dy) const {
    // Central difference, scaled by world units
    float h_xp = sample(wx + cell_scale_, wy);
    float h_xm = sample(wx - cell_scale_, wy);
    float h_yp = sample(wx, wy + cell_scale_);
    float h_ym = sample(wx, wy - cell_scale_);
    dh_dx = (h_xp - h_xm) / (2.0f * cell_scale_);
    dh_dy = (h_yp - h_ym) / (2.0f * cell_scale_);
}

void Heightmap::fill_fractal_noise(uint64_t seed,
                                   float max_height,
                                   float frequency,
                                   int octaves,
                                   float persistence) {
    // Build a fractal terrain by stacking octaves of value noise with
    // increasing frequency and decreasing amplitude (a.k.a. fBm).
    float max_amp = 0.0f;
    {
        float amp = 1.0f;
        for (int o = 0; o < octaves; ++o) {
            max_amp += amp;
            amp *= persistence;
        }
    }

    for (int j = 0; j < size_y_; ++j) {
        for (int i = 0; i < size_x_; ++i) {
            float wx = i * cell_scale_;
            float wy = j * cell_scale_;
            float total = 0.0f;
            float amp = 1.0f;
            float freq = frequency;
            for (int o = 0; o < octaves; ++o) {
                total += value_noise(wx * freq, wy * freq, seed + o * 73856093ULL) * amp;
                amp *= persistence;
                freq *= 2.0f;
            }
            // Normalize to [0,1] then scale to [0, max_height]
            float h = (total / max_amp) * max_height;
            data_[idx(i, j)] = h;
        }
    }
}

}  // namespace dp::world
