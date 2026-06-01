#pragma once

#include "core/interfaces/IPerception.h"

namespace neuro::routes::protagonist {

// L2.7 v13 — Hierarchical RL goal-context input.
// Exposes the protagonist's currently-active high-level goal as a
// 13-onehot to the brain so the worker policy can specialise its
// behaviour to the manager's directive (e.g. "in BuildWorksite goal,
// prefer chop+deposit; in Hunt goal, prefer attack"). The
// MemoryCognitionPerception already exposes a normalised goal_idx
// scalar on dim 32, but a single scalar forces the network to
// reverse-engineer "which discrete goal is this", which fights the
// hierarchical signal. The 13-onehot is the standard
// context-conditioned-policy input from PEARL / RL² in our
// HRL-friendly form.
class GoalContextPerception final : public IPerception {
public:
    // GoalType::Count = 13. We size the perception to that value;
    // increasing the enum requires updating this constant.
    static constexpr std::size_t kDim = 13;

    GoalContextPerception() = default;

    std::size_t dim() const override { return kDim; }
    void sense(const IAgent& self, const IWorld& world, std::span<float> out) const override;
    const char* name() const override { return "goal_context"; }
};

}  // namespace neuro::routes::protagonist
