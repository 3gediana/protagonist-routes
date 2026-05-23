#include "world/FoodSystem.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace dp::world {

float FoodSystem::frand() {
    rng_state_ ^= rng_state_ >> 12;
    rng_state_ ^= rng_state_ << 25;
    rng_state_ ^= rng_state_ >> 27;
    uint64_t r = rng_state_ * 0x2545F4914F6CDD1DULL;
    return static_cast<float>(r & 0xFFFFFFu) / 16777216.0f;
}

void FoodSystem::respawn_at(int idx, const World& world) {
    float wx_max = world.heightmap().world_size_x();
    float wy_max = world.heightmap().world_size_y();
    // Reject samples that fall inside cave volumes - we want surface food.
    for (int attempts = 0; attempts < 16; ++attempts) {
        float x = frand() * wx_max;
        float y = frand() * wy_max;
        float z = world.surface_height(x, y) + 0.3f;
        if (!world.is_underground(x, y, z + 0.5f)) {
            pellets_[idx].x = x;
            pellets_[idx].y = y;
            pellets_[idx].z = z;
            pellets_[idx].alive = true;
            return;
        }
    }
    // Fallback: place even if we couldn't find a non-cave spot
    float x = frand() * wx_max;
    float y = frand() * wy_max;
    pellets_[idx].x = x;
    pellets_[idx].y = y;
    pellets_[idx].z = world.surface_height(x, y) + 0.3f;
    pellets_[idx].alive = true;
}

void FoodSystem::initialize(uint64_t seed, int target_count, const World& world) {
    rng_state_ = seed ? seed : 1;
    target_count_ = target_count;
    pellets_.assign(target_count, Pellet{});
    for (int i = 0; i < target_count; ++i) {
        respawn_at(i, world);
    }
}

int FoodSystem::try_eat(float x, float y, float z, const World& world,
                        float eat_radius) {
    int eaten = 0;
    float r2 = eat_radius * eat_radius;
    for (size_t i = 0; i < pellets_.size(); ++i) {
        auto& p = pellets_[i];
        if (!p.alive) continue;
        float dx = p.x - x;
        float dy = p.y - y;
        float dz = p.z - z;
        if (dx * dx + dy * dy + dz * dz <= r2) {
            ++eaten;
            respawn_at(static_cast<int>(i), world);
        }
    }
    return eaten;
}

std::vector<int> FoodSystem::closest_k(float x, float y, float z, int k) const {
    std::vector<std::pair<float, int>> dists;
    dists.reserve(pellets_.size());
    for (size_t i = 0; i < pellets_.size(); ++i) {
        const auto& p = pellets_[i];
        if (!p.alive) continue;
        float dx = p.x - x;
        float dy = p.y - y;
        float dz = p.z - z;
        dists.emplace_back(dx * dx + dy * dy + dz * dz, static_cast<int>(i));
    }
    std::sort(dists.begin(), dists.end(),
              [](const std::pair<float,int>& a, const std::pair<float,int>& b){
                  return a.first < b.first;
              });

    std::vector<int> out(k, -1);
    for (int i = 0; i < k && i < static_cast<int>(dists.size()); ++i) {
        out[i] = dists[i].second;
    }
    return out;
}

int FoodSystem::alive_count() const {
    int n = 0;
    for (const auto& p : pellets_) if (p.alive) ++n;
    return n;
}

}  // namespace dp::world
