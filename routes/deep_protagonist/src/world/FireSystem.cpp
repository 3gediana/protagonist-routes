#include "world/FireSystem.hpp"

#include <algorithm>
#include <cmath>

namespace dp::world {

bool FireSystem::tend(float x, float y) {
    // Refuel the nearest existing campfire within merge_radius, if any.
    int   best   = -1;
    float best_d2 = cfg_.merge_radius * cfg_.merge_radius;
    for (int i = 0; i < static_cast<int>(fires_.size()); ++i) {
        float dx = fires_[i].x - x;
        float dy = fires_[i].y - y;
        float d2 = dx * dx + dy * dy;
        if (d2 <= best_d2) {
            best_d2 = d2;
            best    = i;
        }
    }
    if (best >= 0) {
        Campfire& f = fires_[best];
        if (f.fuel >= cfg_.fuel_max) return false;  // already full -> no waste
        f.fuel = std::min(cfg_.fuel_max, f.fuel + cfg_.fuel_per_wood);
        f.lit  = true;
        return true;
    }

    // Otherwise build a brand-new lit campfire (respecting the hard cap).
    if (static_cast<int>(fires_.size()) >= cfg_.max_fires) return false;
    Campfire f;
    f.x    = x;
    f.y    = y;
    f.fuel = cfg_.fuel_per_wood;
    f.lit  = true;
    fires_.push_back(f);
    return true;
}

void FireSystem::tick(float dt) {
    for (auto& f : fires_) {
        if (!f.lit) continue;
        f.fuel -= dt;
        if (f.fuel <= 0.0f) {
            f.fuel = 0.0f;
            f.lit  = false;
        }
    }
    // Drop burnt-out fires so the vector stays small and warm_rate_at is cheap.
    fires_.erase(std::remove_if(fires_.begin(), fires_.end(),
                                [](const Campfire& f) {
                                    return !f.lit && f.fuel <= 0.0f;
                                }),
                 fires_.end());
}

float FireSystem::warm_rate_at(float px, float py) const {
    const float r2 = cfg_.warm_radius * cfg_.warm_radius;
    for (const auto& f : fires_) {
        if (!f.lit) continue;
        float dx = f.x - px;
        float dy = f.y - py;
        if (dx * dx + dy * dy <= r2) return cfg_.warm_rate_per_min;
    }
    return 0.0f;
}

int FireSystem::count_lit() const {
    int n = 0;
    for (const auto& f : fires_) {
        if (f.lit) ++n;
    }
    return n;
}

void FireSystem::clear() {
    fires_.clear();
}

}  // namespace dp::world
