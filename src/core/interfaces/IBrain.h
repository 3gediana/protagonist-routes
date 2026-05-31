#pragma once
//
// core/interfaces/IBrain.h
//
// Forward-inference interface for an agent's neural network.
//
// M1: FixedTopologyBrain implements this with a static MLP.
// M3: NeatBrain replaces it with NEAT + CTRNN.
// M5: terrain inputs are appended to the input vector; brain doesn't change.
//
// Critical contract: input/output dimensions can change as the project
// evolves (M5 adds terrain inputs, F-COMM adds signal channels). Brains
// must NOT assume fixed dimensions; they expose inputDim() / outputDim()
// for the world to adapt.

#include "core/types/Aliases.h"

#include <span>
#include <vector>

namespace neuro {

// Optional batched-forward extension. Brains that can split their forward()
// into prepare/dense/finalize phases can implement this so a batched runner
// can fold N agents' dense matmul into a single GPU/SIMD call.
//
// Three-phase contract:
//   Phase 1 (per-agent, parallel): prepareInputs() — perception-side bookkeeping,
//           encode external inputs, build full_input vector. NO dense matmul yet.
//   Phase 2 (single batched call): runner reads denseSlot() from each brain,
//           batches their (input, weights, biases) into one GPU pass, writes
//           hidden / action / interface state back via the slot's mutable spans.
//   Phase 3 (per-agent, parallel): finalizeOutputs(dt) — DNC step / working memory
//           updates, returns the action span.
//
// All dimensions returned by denseSlot() must be stable for the brain's lifetime.
struct BatchedDenseSlot {
    std::size_t total_input_dim{0};
    std::size_t hidden_dim{0};
    std::size_t action_dim{0};
    std::size_t interface_dim{0};

    float hidden_alpha{0.0f};
    float action_alpha{0.0f};

    std::span<const float> full_input;

    std::span<const float> input_hidden_weights;
    std::span<const float> hidden_bias;
    std::span<const float> hidden_action_weights;
    std::span<const float> action_bias;
    std::span<const float> hidden_interface_weights;
    std::span<const float> interface_bias;

    // Optional post-activation addend applied to each hidden unit's target
    // BEFORE the leaky integration step, i.e. target = tanh(z_h) + addend[h].
    // This mirrors the CPU full-path forward(), which injects
    // 0.15 * working_memory.summary()[h % 8] into motor_target. The batched
    // dense math must apply the same term so it is bit-identical to the
    // per-agent CPU forward(). If empty, no addend is applied (zero term).
    std::span<const float> hidden_target_addend;

    std::span<float> hidden_io;
    std::span<float> action_io;
    std::span<float> interface_out;
};

class IBatchableBrain {
public:
    virtual ~IBatchableBrain() = default;

    virtual void prepareInputs(std::span<const float> external_inputs, float dt) = 0;
    virtual BatchedDenseSlot denseSlot() = 0;
    virtual std::span<const float> finalizeOutputs(float dt) = 0;
};

class IBrain {
public:
    virtual ~IBrain() = default;

    // The neural net's expected input width. Set at construction time
    // (typically derived from the genome). Stable for the brain's lifetime.
    virtual std::size_t inputDim()  const = 0;
    virtual std::size_t outputDim() const = 0;

    // Run one forward step.
    //   inputs.size() must equal inputDim()
    //   returned span has outputDim() elements
    //   dt: simulation step (used by CTRNN; ignored by feedforward brains)
    //
    // Implementations may keep internal state between calls (CTRNN, RNN);
    // call reset() to clear it (e.g. when an agent is reset between trials).
    virtual std::span<const float> forward(std::span<const float> inputs,
                                           float dt) = 0;

    // Clear internal state (recurrent activations, accumulators).
    virtual void reset() = 0;

    // Coarse complexity metric, e.g. parameter count. Used for stats.
    virtual std::size_t parameterCount() const = 0;

    // Optional: brains that support batched dense forward return a non-null
    // pointer to themselves cast as IBatchableBrain. Default: not batchable.
    virtual IBatchableBrain* asBatchable() { return nullptr; }
};

}  // namespace neuro
