#include "world/BiomeMap.hpp"

#include <algorithm>
#include <cmath>

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
    // Find the heightmap's max so we can normalize height into [0,1].
    float h_max = 1e-6f;
    for (float h : hm.raw()) {
        if (h > h_max) h_max = h;
    }
    (void)sea_level;

    const float moisture_freq = 0.0015f;  // larger features than terrain
    const int   moisture_octaves = 4;
    const float moisture_persistence = 0.55f;

    for (int j = 0; j < size_y_; ++j) {
        for (int i = 0; i < size_x_; ++i) {
            // Same world coords as the heightmap: cell (i,j) -> wx=i*scale
            // The cell scale is the heightmap's, so we just use grid idx.
            float h_norm = hm.at(i, j) / h_max;  // [0, 1]
            float moisture = fbm(static_cast<float>(i) * moisture_freq,
                                 static_cast<float>(j) * moisture_freq,
                                 seed ^ 0xBEEFULL,
                                 moisture_octaves,
                                 moisture_persistence);

            Biome b;
            if (h_norm > mountain_threshold) {
                b = Biome::Mountain;
            } else if (moisture < dry_threshold && h_norm < 0.55f) {
                b = Biome::Desert;
            } else if (moisture > wet_threshold && h_norm < 0.45f) {
                b = Biome::Swamp;
            } else if (moisture > 0.45f && h_norm < 0.6f) {
                b = Biome::Forest;
            } else {
                b = Biome::Plain;
            }
            data_[idx(i, j)] = static_cast<uint8_t>(b);
        }
    }
}

}  // namespace dp::world
