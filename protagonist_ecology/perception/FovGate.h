#pragma once

#include "core/interfaces/IAgent.h"
#include "core/interfaces/IWorld.h"
#include "core/types/Vec2.h"
#include "core/world/layers/terrain/TerrainLayer.h"

#include <cmath>

namespace neuro::routes::protagonist {

// Phase C (one-pot integration): limited-perception parameters carried by the
// exteroceptive perceptions (currently DangerPerception/fire; later
// AgentSocial/Water/Tree). Computed once per agent at createProtagonist from the
// annealed schedule (difficulty = f(generation)) and held constant for the
// agent's life within that generation.
struct FovParams {
    bool  enabled{false};
    float cos_half_angle{-1.0f};  // cos(180deg) = -1 => no angular restriction
    bool  occlusion{false};
};

// Shared limited-perception gate. Returns whether a world target is currently
// perceivable to `self`. Two gates:
//   1. View cone  - when MOVING, the target must lie within +/- half-angle of
//                   the velocity heading (cos(angle) >= cos_half_angle). When
//                   (near) stationary the agent looks around freely (360 deg),
//                   so resting to scan is a valid, learnable behaviour.
//   2. Occlusion  - terrain line-of-sight via TerrainLayer::isVisible (already
//                   used by TerrainPerception), so hills/ridges block sight.
// Targets that fail either gate are written as `not sensed` (zeros) by the
// caller, forcing reliance on spatial memory + temporal prediction (the
// internal clock) instead of always-on omniscient sensing.
//
// Deterministic: depends only on agent position/velocity and static terrain, so
// it is safe under adt=1 replay. Perceptual NOISE is intentionally NOT added
// here (would need a deterministic per-(agent,tick) RNG); add later if needed.
inline bool fovPerceivable(const IAgent& self, Vec2 target, const IWorld& world,
                           float cos_half_angle, bool occlusion) {
    const Vec2 sp = self.position();
    const Vec2 d{target.x - sp.x, target.y - sp.y};
    const float dist = std::sqrt(d.x * d.x + d.y * d.y);
    if (dist > 1e-4f) {
        const Vec2 v = self.velocity();
        const float speed = std::sqrt(v.x * v.x + v.y * v.y);
        if (speed > 0.01f) {  // moving: restrict to forward view cone
            const float cosang = (d.x * v.x + d.y * v.y) / (dist * speed);
            if (cosang < cos_half_angle) {
                return false;
            }
        }
        // stationary: no angular restriction (looking around)
    }
    if (occlusion) {
        if (const auto* terrain = world.getLayer<TerrainLayer>(); terrain != nullptr) {
            if (!terrain->isVisible(sp, target)) {
                return false;
            }
        }
    }
    return true;
}

// Convenience overload taking FovParams. When the gate is disabled the target
// is always perceivable (preserves pre-Phase-C behaviour exactly).
inline bool fovPerceivable(const IAgent& self, Vec2 target, const IWorld& world,
                           const FovParams& p) {
    if (!p.enabled) {
        return true;
    }
    return fovPerceivable(self, target, world, p.cos_half_angle, p.occlusion);
}

}  // namespace neuro::routes::protagonist
