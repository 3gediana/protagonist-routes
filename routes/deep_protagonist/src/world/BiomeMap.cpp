#include "world/BiomeMap.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace dp::world {

const char* biome_name(Biome b) {
    switch (b) {
        case Biome::Plain:    return "plain";
        case Biome::Forest:   return "forest";
        case Biome::Mountain: return "mountain";
        case Biome::River:    return "river";
        case Biome::Swamp:    return "swamp";
        case Biome::Desert:   return "desert";
        default:              return "?";
    }
}

namespace {

// Same value-noise primitive used by Heightmap, duplicated locally to keep
// the two systems decoupled.
inline float hash2(int i, int j, uint64_t seed) {
    uint64_t x = static_cast<uint64_t>(i) * 0x9E3779B97F4A7C15ULL
               ^ static_cast<uint64_t>(j) * 0xC2B2AE3D27D4EB4FULL
               ^ seed * 0x165667B19E3779F9ULL;
    x ^= x >> 27;
    x *= 0x94D049BB133111EBULL;
    x ^= x >> 31;
    return static_cast<float>(x & 0xFFFFFFu) / 16777216.0f;
}

inline float fade(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

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

float fbm(float x, float y, uint64_t seed, int octaves, float persistence) {
    float total = 0.0f;
    float amp = 1.0f;
    float freq = 1.0f;
    float max_amp = 0.0f;
    for (int o = 0; o < octaves; ++o) {
        total += value_noise(x * freq, y * freq, seed + o * 73856093ULL) * amp;
        max_amp += amp;
        amp *= persistence;
        freq *= 2.0f;
    }
    return total / max_amp;
}

}  // namespace

BiomeMap::BiomeMap(int size_x, int size_y)
    : size_x_(size_x), size_y_(size_y),
      data_(static_cast<size_t>(size_x) * static_cast<size_t>(size_y),
            static_cast<uint8_t>(Biome::Plain)) {}

Biome BiomeMap::at(int i, int j) const {
    i = std::clamp(i, 0, size_x_ - 1);
    j = std::clamp(j, 0, size_y_ - 1);
    return static_cast<Biome>(data_[idx(i, j)]);
}

Biome BiomeMap::sample(float wx, float wy, float cell_scale) const {
    int i = static_cast<int>(std::floor(wx / cell_scale));
    int j = static_cast<int>(std::floor(wy / cell_scale));
    return at(i, j);
}

void BiomeMap::mark_river(int i, int j) {
    if (i < 0 || i >= size_x_ || j < 0 || j >= size_y_) return;
    data_[idx(i, j)] = static_cast<uint8_t>(Biome::River);
}

void BiomeMap::mark_swamp(int i, int j) {
    if (i < 0 || i >= size_x_ || j < 0 || j >= size_y_) return;
    if (data_[idx(i, j)] == static_cast<uint8_t>(Biome::River)) return;
    data_[idx(i, j)] = static_cast<uint8_t>(Biome::Swamp);
}

void BiomeMap::build(const Heightmap& hm, uint64_t seed,
                     float sea_level,
                     float mountain_threshold,
                     float wet_threshold,
                     float dry_threshold) {
    // The legacy fixed thresholds barely produced any desert/swamp because the
    // moisture noise rarely crossed them, and ~half the (flat) map fell over a
    // single height threshold so it read as "mostly mountain". Instead we build
    // a Whittaker-style climate map from three fields - elevation, temperature
    // (warm equator + altitude cooling + noise) and moisture - and slice them
    // by PERCENTILE so the biome MIX is stable and diverse on every seed, while
    // the smooth low-frequency noise keeps each biome in contiguous regions
    // scattered across the whole map (not one pocket, not all desert).
    (void)sea_level; (void)mountain_threshold; (void)wet_threshold; (void)dry_threshold;

    const int N = size_x_ * size_y_;
    float h_max = 1e-6f;
    for (float h : hm.raw()) if (h > h_max) h_max = h;

    const float scale      = hm.cell_scale();
    const float moist_freq = 0.004f;  // ~250 m moisture provinces
    const float temp_freq  = 0.003f;  // ~330 m thermal provinces

    std::vector<float> hN(N), moi(N), tmp(N);
    for (int j = 0; j < size_y_; ++j) {
        for (int i = 0; i < size_x_; ++i) {
            int id = idx(i, j);
            float h_norm = hm.at(i, j) / h_max;             // [0,1]
            float wx = static_cast<float>(i) * scale;
            float wy = static_cast<float>(j) * scale;
            float m = fbm(wx * moist_freq, wy * moist_freq,
                          seed ^ 0xBEEFULL, 4, 0.55f);        // [0,1]
            // Latitude: south (small j) warm, north (large j) cold.
            float lat = static_cast<float>(j) / static_cast<float>(size_y_ - 1);
            float t_noise = fbm(wx * temp_freq, wy * temp_freq,
                                seed ^ 0x7A1EULL, 3, 0.5f);   // [0,1]
            // Warmer near the equator and at low altitude; jittered by noise.
            float t = (1.0f - lat) * 0.6f + t_noise * 0.4f - h_norm * 0.5f;
            hN[id] = h_norm;
            moi[id] = m;
            tmp[id] = t;
        }
    }

    // Percentile cutoff over a copy (N small enough that 6 partial sorts are cheap).
    auto pct = [](std::vector<float> v, float p) {
        size_t k = static_cast<size_t>(p * static_cast<float>(v.size() - 1));
        std::nth_element(v.begin(), v.begin() + k, v.end());
        return v[k];
    };
    const float h_mtn = pct(hN, 0.82f);  // top ~18% elevation -> mountains
    const float h_low = pct(hN, 0.35f);  // lowest ~35% -> basins (swamp candidates)
    const float m_wet = pct(moi, 0.72f); // wettest ~28%
    const float m_mid = pct(moi, 0.45f);
    const float m_dry = pct(moi, 0.28f); // driest ~28%
    const float t_hot = pct(tmp, 0.55f); // warmer ~45%

    for (int id = 0; id < N; ++id) {
        float h_norm = hN[id], m = moi[id], t = tmp[id];
        Biome b;
        if (h_norm >= h_mtn) {
            b = Biome::Mountain;                       // high ridges & peaks
        } else if (m >= m_wet && h_norm <= h_low) {
            b = Biome::Swamp;                          // wet lowland basins
        } else if (m <= m_dry && t >= t_hot) {
            b = Biome::Desert;                         // hot & dry
        } else if (m >= m_mid) {
            b = Biome::Forest;                         // moister mid-elevation
        } else {
            b = Biome::Plain;                          // grassland fills the rest
        }
        data_[id] = static_cast<uint8_t>(b);
    }
}

}  // namespace dp::world
