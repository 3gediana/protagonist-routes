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

// Hermite smoothstep: 0 below edge0, 1 above edge1, smooth in between.
inline float smoothstep(float edge0, float edge1, float x) {
    float t = (x - edge0) / (edge1 - edge0);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return t * t * (3.0f - 2.0f * t);
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

namespace {

// Fractional Brownian motion (rolling hills): octaves of value noise summed
// with shrinking amplitude. Returns ~[0,1].
float fbm(float x, float y, uint64_t seed, int octaves, float freq, float pers) {
    float total = 0.0f, amp = 1.0f, maxamp = 0.0f, f = freq;
    for (int o = 0; o < octaves; ++o) {
        total += value_noise(x * f, y * f, seed + o * 73856093ULL) * amp;
        maxamp += amp;
        amp *= pers;
        f *= 2.0f;
    }
    return maxamp > 0.0f ? total / maxamp : 0.0f;
}

// Ridged multifractal (sharp mountain ridges): folds noise around its midline
// so the *creases* become peaks, then squares to sharpen them. Returns ~[0,1].
float ridged(float x, float y, uint64_t seed, int octaves, float freq, float pers) {
    float total = 0.0f, amp = 1.0f, maxamp = 0.0f, f = freq;
    for (int o = 0; o < octaves; ++o) {
        float n = value_noise(x * f, y * f, seed + o * 19349663ULL);  // [0,1]
        n = 1.0f - std::fabs(2.0f * n - 1.0f);  // ridge: peak at n==0.5
        n = n * n;                              // sharpen the crest
        total += n * amp;
        maxamp += amp;
        amp *= pers;
        f *= 2.0f;
    }
    return maxamp > 0.0f ? total / maxamp : 0.0f;
}

}  // namespace

void Heightmap::fill_fractal_noise(uint64_t seed,
                                   float max_height,
                                   float frequency,
                                   int octaves,
                                   float persistence) {
    // Real-relief terrain: combine three layers so the world reads as
    // "plains + foothills + mountain RANGES + valleys/basins" instead of a
    // uniform field of gentle bumps.
    //   1) base fBm        - continent-scale rolling ground everywhere
    //   2) mountain mask    - smooth low-freq field that decides WHERE ranges
    //                         are allowed to rise (so mountains form bands /
    //                         massifs, not an even sprinkle of peaks)
    //   3) ridged layer     - sharp ridgelines, gated by the mask
    // A redistribution exponent then flattens the lowlands (wide walkable
    // plains + basins) while exaggerating the highs (dramatic summits).
    for (int j = 0; j < size_y_; ++j) {
        for (int i = 0; i < size_x_; ++i) {
            float wx = i * cell_scale_;
            float wy = j * cell_scale_;

            float base = fbm(wx, wy, seed, octaves, frequency, persistence);
            // Low-freq mask: where ranges occur. Half the base frequency so
            // mountain belts are several hundred metres wide.
            float mask = fbm(wx, wy, seed ^ 0x51EDC0DEULL, 3,
                             frequency * 0.5f, 0.5f);
            float gate = smoothstep(0.50f, 0.78f, mask);  // 0 plains -> 1 range core
            float mtn  = ridged(wx, wy, seed ^ 0xA17E57EDULL, octaves,
                                frequency * 1.6f, 0.5f);

            // Plains follow the gentle base; ranges add tall sharp ridges.
            float e = base * 0.55f + gate * mtn * 0.75f;
            if (e < 0.0f) e = 0.0f;
            if (e > 1.0f) e = 1.0f;
            // Redistribution: carve flat lowlands, steepen the peaks.
            e = std::pow(e, 1.7f);

            data_[idx(i, j)] = e * max_height;
        }
    }
}

}  // namespace dp::world
