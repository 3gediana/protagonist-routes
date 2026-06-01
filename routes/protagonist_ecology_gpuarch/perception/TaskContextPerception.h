#pragma once

#include "core/interfaces/IPerception.h"

namespace neuro::routes::protagonist {

// L2.6 v12 — context-conditioned policy (PEARL/RL² inspired).
// Exposes the current task_template index as an N-onehot input to the
// protagonist brain so a single network can learn task-specific
// behaviour (proactive_build vs solo_hunt vs mixed_ecology) without
// needing to delete templates from the pool. The Runtime calls
// agent->setTaskTemplateIndex(...) after applyTaskTemplate() picks a
// template for the current episode. If the index is -1 or out of
// range, all kDim slots are 0 (graceful degradation for runs without
// task_templates).
class TaskContextPerception final : public IPerception {
public:
    // Conservative max template count — covers the 6 templates in
    // l2_4_curriculum / v10_real.toml plus 2 buffer slots. Picked at
    // compile time so the brain input_dim stays stable across runs
    // even if the toml adds/removes templates between checkpoints.
    static constexpr std::size_t kDim = 8;

    TaskContextPerception() = default;

    std::size_t dim() const override { return kDim; }
    void sense(const IAgent& self, const IWorld& world, std::span<float> out) const override;
    const char* name() const override { return "task_context"; }
};

}  // namespace neuro::routes::protagonist
