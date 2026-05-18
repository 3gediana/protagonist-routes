#include "core/world/layers/terrain/TerrainGenerator.h"

#include <cmath>
#include <random>

namespace neuro {

namespace {

class PerlinNoise {
public:
    explicit PerlinNoise(std::uint32_t seed) {
        std::mt19937 rng(seed);
        perm_.resize(512);
        for (int i = 0; i < 256; ++i) {
            perm_[i] = i;
        }
        for (int i = 255; i > 0; --i) {
            std::uniform_int_distribution<int> dist(0, i);
            const int j = dist(rng);
            std::swap(perm_[i], perm_[j]);
        }
        for (int i = 0; i < 256; ++i) {
            perm_[256 + i] = perm_[i];
        }
    }

    float noise(float x, float y) const {
        const int xi = static_cast<int>(std::floor(x)) & 255;
        const int yi = static_cast<int>(std::floor(y)) & 255;
        const float xf = x - std::floor(x);
        const float yf = y - std::floor(y);

        const float u = fade(xf);
        const float v = fade(yf);

        const int aa = perm_[perm_[xi] + yi];
        const int ab = perm_[perm_[xi] + yi + 1];
        const int ba = perm_[perm_[xi + 1] + yi];
        const int bb = perm_[perm_[xi + 1] + yi + 1];

        const float x1 = lerp(grad(aa, xf, yf), grad(ba, xf - 1, yf), u);
        const float x2 = lerp(grad(ab, xf, yf - 1), grad(bb, xf - 1, yf - 1), u);

        return lerp(x1, x2, v);
    }

    float octaveNoise(float x, float y, int octaves, float persistence) const {
        float total = 0.0f;
        float frequency = 1.0f;
        float amplitude = 1.0f;
        float max_amplitude = 0.0f;

        for (int i = 0; i < octaves; ++i) {
            total += noise(x * frequency, y * frequency) * amplitude;
            max_amplitude += amplitude;
            amplitude *= persistence;
            frequency *= 2.0f;
        }

        return total / max_amplitude;
    }

private:
    static float fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
    static float lerp(float a, float b, float t) { return a + t * (b - a); }
    static float grad(int hash, float x, float y) {
        const int h = hash & 3;
        const float u = (h & 1) == 0 ? x : -x;
        const float v = (h & 2) == 0 ? y : -y;
        return u + v;
    }

    std::vector<int> perm_;
};

}  // namespace

void TerrainGenerator::generateHeightField(HeightField& hf, const TerrainParams& params, std::uint32_t seed) {
    PerlinNoise perlin(seed);

    for (std::size_t y = 0; y < hf.height(); ++y) {
        for (std::size_t x = 0; x < hf.width(); ++x) {
            const float nx = static_cast<float>(x) / static_cast<float>(hf.width()) * params.perlin_frequency;
            const float ny = static_cast<float>(y) / static_cast<float>(hf.height()) * params.perlin_frequency;

            float h = perlin.octaveNoise(nx, ny, params.perlin_octaves, params.perlin_persistence);
            h = (h + 1.0f) * 0.5f;
            h = std::clamp(h, 0.0f, 1.0f);
            hf.at(x, y) = h * params.height_scale;
        }
    }
}

void TerrainGenerator::generateCaveMask(CaveMask& mask, const TerrainParams& params, std::uint32_t seed) {
    PerlinNoise perlin(seed + 7777);
    std::mt19937 rng(seed + 9999);

    for (std::size_t y = 0; y < mask.height(); ++y) {
        for (std::size_t x = 0; x < mask.width(); ++x) {
            const float nx = static_cast<float>(x) / static_cast<float>(mask.width()) * 6.0f;
            const float ny = static_cast<float>(y) / static_cast<float>(mask.height()) * 6.0f;

            float n = perlin.octaveNoise(nx, ny, 3, 0.5f);
            n = (n + 1.0f) * 0.5f;

            const float threshold = 1.0f - params.cave_density;
            mask.set(x, y, n > threshold);
        }
    }
}

}  // namespace neuro
