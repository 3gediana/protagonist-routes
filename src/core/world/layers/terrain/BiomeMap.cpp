#include "core/world/layers/terrain/BiomeMap.h"

#include "core/world/layers/terrain/HeightField.h"

#include <algorithm>
#include <cmath>

namespace neuro {

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

// Value-noise primitive (same construction the deep_protagonist BiomeMap uses),
// kept local so the ecology climate fields stay decoupled from the Perlin used
// for the height field / cave mask.
inline float hash2(int i, int j, std::uint64_t seed) {
    std::uint64_t x = static_cast<std::uint64_t>(i) * 0x9E3779B97F4A7C15ULL
                    ^ static_cast<std::uint64_t>(j) * 0xC2B2AE3D27D4EB4FULL
                    ^ seed * 0x165667B19E3779F9ULL;
    x ^= x >> 27;
    x *= 0x94D049BB133111EBULL;
    x ^= x >> 31;
    return static_cast<float>(x & 0xFFFFFFu) / 16777216.0f;
}

inline float fade(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

inline float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

float value_noise(float x, float y, std::uint64_t seed) {
    int xi = static_cast<int>(std::floor(x));
    int yi = static_cast<int>(std::floor(y));
    float xf = x - static_cast<float>(xi);
    float yf = y - static_cast<float>(yi);
    float u = fade(xf);
    float v = fade(yf);
    float v00 = hash2(xi,     yi,     seed);
    float v10 = hash2(xi + 1, yi,     seed);
    float v01 = hash2(xi,     yi + 1, seed);
    float v11 = hash2(xi + 1, yi + 1, seed);
    return lerpf(lerpf(v00, v10, u), lerpf(v01, v11, u), v);
}

float fbm(float x, float y, std::uint64_t seed, int octaves, float persistence) {
    float total = 0.0f;
    float amp = 1.0f;
    float freq = 1.0f;
    float max_amp = 0.0f;
    for (int o = 0; o < octaves; ++o) {
        total += value_noise(x * freq, y * freq, seed + static_cast<std::uint64_t>(o) * 73856093ULL) * amp;
        max_amp += amp;
        amp *= persistence;
        freq *= 2.0f;
    }
    return max_amp > 0.0f ? total / max_amp : 0.0f;
}

}  // namespace

BiomeMap::BiomeMap(std::size_t size_x, std::size_t size_y, float cell_scale)
    : size_x_(size_x),
      size_y_(size_y),
      cell_scale_(cell_scale > 0.0f ? cell_scale : 1.0f),
      data_(size_x * size_y, static_cast<std::uint8_t>(Biome::Plain)) {}

Biome BiomeMap::at(int i, int j) const {
    if (size_x_ == 0 || size_y_ == 0) return Biome::Plain;
    i = std::clamp(i, 0, static_cast<int>(size_x_) - 1);
    j = std::clamp(j, 0, static_cast<int>(size_y_) - 1);
    return static_cast<Biome>(data_[idx(i, j)]);
}

Biome BiomeMap::sample(float wx, float wy) const {
    if (size_x_ == 0 || size_y_ == 0) return Biome::Plain;
    int i = static_cast<int>(std::floor(wx / cell_scale_));
    int j = static_cast<int>(std::floor(wy / cell_scale_));
    return at(i, j);
}

void BiomeMap::build(const HeightField& hm, std::uint64_t seed) {
    size_x_ = hm.width();
    size_y_ = hm.height();
    cell_scale_ = hm.cellSize() > 0.0f ? hm.cellSize() : 1.0f;
    data_.assign(size_x_ * size_y_, static_cast<std::uint8_t>(Biome::Plain));

    const std::size_t N = size_x_ * size_y_;
    if (N == 0) return;

    float h_max = 1e-6f;
    for (std::size_t j = 0; j < size_y_; ++j) {
        for (std::size_t i = 0; i < size_x_; ++i) {
            const float h = hm.at(i, j);
            if (h > h_max) h_max = h;
        }
    }

    const float moist_freq = 0.004f;  // ~250 m moisture provinces
    const float temp_freq  = 0.003f;  // ~330 m thermal provinces

    std::vector<float> hN(N), moi(N), tmp(N);
    for (std::size_t j = 0; j < size_y_; ++j) {
        for (std::size_t i = 0; i < size_x_; ++i) {
            const std::size_t id = idx(static_cast<int>(i), static_cast<int>(j));
            const float h_norm = hm.at(i, j) / h_max;            // [0,1]
            const float wx = static_cast<float>(i) * cell_scale_;
            const float wy = static_cast<float>(j) * cell_scale_;
            const float m = fbm(wx * moist_freq, wy * moist_freq,
                                seed ^ 0xBEEFULL, 4, 0.55f);       // [0,1]
            const float lat = static_cast<float>(j) / static_cast<float>(std::max<std::size_t>(1, size_y_ - 1));
            const float t_noise = fbm(wx * temp_freq, wy * temp_freq,
                                      seed ^ 0x7A1EULL, 3, 0.5f);  // [0,1]
            // Warmer near the (southern) equator and at low altitude; jittered.
            const float t = (1.0f - lat) * 0.6f + t_noise * 0.4f - h_norm * 0.5f;
            hN[id] = h_norm;
            moi[id] = m;
            tmp[id] = t;
        }
    }

    auto pct = [](std::vector<float> v, float p) {
        if (v.empty()) return 0.0f;
        std::size_t k = static_cast<std::size_t>(p * static_cast<float>(v.size() - 1));
        std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(k), v.end());
        return v[k];
    };
    const float h_mtn   = pct(hN, 0.82f);  // top ~18% elevation -> mountains
    const float h_river = pct(hN, 0.08f);  // lowest ~8% -> river basins
    const float h_low   = pct(hN, 0.35f);  // lowest ~35% -> swamp candidates
    const float m_wet   = pct(moi, 0.72f);
    const float m_mid   = pct(moi, 0.45f);
    const float m_dry   = pct(moi, 0.28f);
    const float t_hot   = pct(tmp, 0.55f);

    for (std::size_t id = 0; id < N; ++id) {
        const float h_norm = hN[id], m = moi[id], t = tmp[id];
        Biome b;
        if (h_norm >= h_mtn) {
            b = Biome::Mountain;                        // high ridges & peaks
        } else if (h_norm <= h_river) {
            b = Biome::River;                           // lowest basins hold water
        } else if (m >= m_wet && h_norm <= h_low) {
            b = Biome::Swamp;                           // wet lowland basins
        } else if (m <= m_dry && t >= t_hot) {
            b = Biome::Desert;                          // hot & dry
        } else if (m >= m_mid) {
            b = Biome::Forest;                          // moister mid-elevation
        } else {
            b = Biome::Plain;                           // grassland fills the rest
        }
        data_[id] = static_cast<std::uint8_t>(b);
    }
}

float BiomeMap::treeWeight(Biome b) {
    switch (b) {
        case Biome::Forest:   return 1.00f;
        case Biome::Swamp:    return 0.45f;
        case Biome::Plain:    return 0.30f;
        case Biome::Mountain: return 0.12f;
        case Biome::River:    return 0.08f;
        case Biome::Desert:   return 0.03f;
        default:              return 0.20f;
    }
}

float BiomeMap::waterWeight(Biome b) {
    switch (b) {
        case Biome::River:    return 1.00f;
        case Biome::Swamp:    return 0.85f;
        case Biome::Mountain: return 0.18f;  // springs / snowmelt
        case Biome::Forest:   return 0.20f;
        case Biome::Plain:    return 0.12f;
        case Biome::Desert:   return 0.00f;
        default:              return 0.15f;
    }
}

float BiomeMap::treeSuitability(float wx, float wy) const {
    return treeWeight(sample(wx, wy));
}

float BiomeMap::waterSuitability(float wx, float wy) const {
    return waterWeight(sample(wx, wy));
}

}  // namespace neuro
