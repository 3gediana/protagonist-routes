#pragma once

#include "core/interfaces/IAgentCapability.h"
#include "core/types/Aliases.h"
#include "core/types/Vec2.h"

#include <atomic>
#include <cstddef>

namespace neuro::routes::protagonist {

// Continuous-time chop capability.
//
// Per-agent instance keeps a "chop session" that spans multiple ticks.
// Each tick of held-down chop output that lands on the same tree extends
// the session; switching trees, dropping the output, or losing range
// closes the session (publishing ChopAbandoned). When the tree falls,
// ChopCompleted is published with the full session duration so memory
// and credit-assignment layers can reconstruct who chopped what for
// how long.
class ChopTreeCapability final : public IAgentCapability {
public:
    ChopTreeCapability(float chop_radius, float damage, float energy_cost, float activation_threshold = 0.5f);

    std::size_t consumedOutputs() const override { return 1; }
    void apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) override;
    const char* name() const override { return "chop_tree"; }

    static void resetCounters() {
        chop_attempts.store(0);
        chop_successes.store(0);
        chop_started_events.store(0);
        chop_completed_events.store(0);
    }

    static inline std::atomic<std::size_t> chop_attempts{0};
    static inline std::atomic<std::size_t> chop_successes{0};
    static inline std::atomic<std::size_t> chop_started_events{0};
    static inline std::atomic<std::size_t> chop_completed_events{0};

private:
    void closeSession(IAgent& agent, IWorld& world, float remaining_integrity);

    float chop_radius_;
    float damage_;
    float energy_cost_;
    float activation_threshold_;

    // Per-agent session state.
    std::size_t session_tree_id_ = 0;            // 0 means: no active session
    double session_started_seconds_ = 0.0;
    double session_last_active_seconds_ = 0.0;
};

}  // namespace neuro::routes::protagonist
