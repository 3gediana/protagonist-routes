#pragma once

// One-pot reserved action outputs (PHASE D/E placeholders).
//
// Reserves a fixed block of brain action outputs for not-yet-activated phase
// capabilities (D feed-offspring x1, E mark-territory x1 + evict-intruder x1)
// so the network's action_dim is locked once and future phases reuse the same
// checkpoint instead of being retrained from scratch.
//
// Passive no-op: it consumes its reserved width so the capability stack fully
// covers the brain output vector (the sum of consumedOutputs() must equal the
// brain outputDim()), but applies no world effect. The corresponding genome
// weights are zero (see ProtagonistGenome::extendActionDim), so these outputs
// are identically 0 until a real capability replaces this stub and evolution
// grows the weights from zero.

#include "core/interfaces/IAgentCapability.h"

#include <cstddef>

namespace neuro::routes::protagonist {

class ReservedActionCapability final : public IAgentCapability {
public:
    explicit ReservedActionCapability(std::size_t reserved_outputs)
        : reserved_outputs_(reserved_outputs) {}

    std::size_t consumedOutputs() const override { return reserved_outputs_; }
    void apply(IAgent& /*agent*/, IWorld& /*world*/, std::span<const float> /*outputs*/, float /*dt*/) override {}
    const char* name() const override { return "reserved_action_slots"; }

private:
    std::size_t reserved_outputs_;
};

}  // namespace neuro::routes::protagonist
