#pragma once

// Phase D/E one-pot action ports, now carrying REAL effects (2026-06).
//
// These three reserved brain outputs used to be a pure no-op
// (ReservedActionCapability). They now drive real social actions computed from
// data already in the sim, so this is the "do it for real, just don't activate
// it" counterpart to the Kin/Territory/WorldHistory perception channels.
//   [0] feed-kin (D)       -- share surplus energy with the nearest close
//                             relative (real cost to self, real gain to kin).
//   [1] mark-territory (E) -- drop/refresh my own decaying scent-mark on the
//                             ground here (real energy cost; recorded in the
//                             lightweight TerritoryMarkLayer). Claims land I can
//                             then defend via evict.
//   [2] evict-intruder (E) -- shove the nearest non-kin intruder off land I
//                             hold: my colony base OR territory I've marked
//                             (real displacement outward).
//
// The genome weights feeding these outputs are zeroed by the one-pot surgery
// (ProtagonistGenome::extendActionDim), so every output stays identically 0
// until a PE run grows them. Activation-day behavior is therefore unchanged --
// exactly like the perception channels -- and turning these on is a matter of
// (re)starting PE evolution, which is a separate, user-gated step.

#include "core/interfaces/IAgentCapability.h"

#include <atomic>
#include <cstddef>

namespace neuro::routes::protagonist {

class SocialActionCapability final : public IAgentCapability {
public:
    explicit SocialActionCapability(std::size_t reserved_outputs,
                                    float sense_radius,
                                    float activation_threshold = 0.5f,
                                    float feed_amount = 8.0f,
                                    float feed_min_surplus = 40.0f,
                                    float evict_push = 4.0f,
                                    float mark_min_energy = 5.0f,
                                    float mark_energy_cost = 2.0f)
        : reserved_outputs_(reserved_outputs),
          sense_radius_(sense_radius),
          activation_threshold_(activation_threshold),
          feed_amount_(feed_amount),
          feed_min_surplus_(feed_min_surplus),
          evict_push_(evict_push),
          mark_min_energy_(mark_min_energy),
          mark_energy_cost_(mark_energy_cost) {}

    std::size_t consumedOutputs() const override { return reserved_outputs_; }
    void apply(IAgent& agent, IWorld& world, std::span<const float> outputs, float dt) override;
    const char* name() const override { return "social_action"; }

    static void resetCounters() {
        feed_attempts.store(0);
        feed_successes.store(0);
        evict_attempts.store(0);
        evict_successes.store(0);
        mark_attempts.store(0);
        mark_successes.store(0);
    }

    static inline std::atomic<std::size_t> feed_attempts{0};
    static inline std::atomic<std::size_t> feed_successes{0};
    static inline std::atomic<std::size_t> evict_attempts{0};
    static inline std::atomic<std::size_t> evict_successes{0};
    static inline std::atomic<std::size_t> mark_attempts{0};
    static inline std::atomic<std::size_t> mark_successes{0};

private:
    std::size_t reserved_outputs_;
    float sense_radius_;
    float activation_threshold_;
    float feed_amount_;
    float feed_min_surplus_;
    float evict_push_;
    float mark_min_energy_;
    float mark_energy_cost_;
};

}  // namespace neuro::routes::protagonist
