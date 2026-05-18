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
};

}  // namespace neuro
