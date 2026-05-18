#pragma once
//
// core/interfaces/IAgentCapability.h
//
// An "actuator" that consumes some prefix of the brain's output vector
// and applies its effect to the world. Multiple capabilities are stacked
// after each other; the sum of their consumed dims must equal the
// brain's outputDim().
//
// Examples:
//   MoveCapability        : consumes (vx, vy)        2 outputs
//   EatCapability         : reads world; consumes 0 (passive trigger)
//   AttackCapability      : (intensity, dir.x, dir.y) 3 outputs (F-FIGHT)
//   EmitSignalCapability  : K signal channels         K outputs (F-COMM)

#include <span>

namespace neuro {

class IAgent;
class IWorld;

class IAgentCapability {
public:
    virtual ~IAgentCapability() = default;

    // How many output floats this capability consumes. May be 0 for
    // passive capabilities (e.g. EatCapability triggers from world state,
    // not brain output).
    virtual std::size_t consumedOutputs() const = 0;

    // Apply this capability for the given agent. `outputs` is the slice
    // of brain outputs assigned to this capability, exactly
    // consumedOutputs() floats wide.
    virtual void apply(IAgent& agent,
                       IWorld& world,
                       std::span<const float> outputs,
                       float dt) = 0;

    // Identifier for logging / debugging.
    virtual const char* name() const = 0;
};

}  // namespace neuro
