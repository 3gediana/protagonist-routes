#pragma once
//
// core/interfaces/IPerception.h
//
// A "sensor" that writes some number of floats into the brain's input
// vector. Multiple perceptions are stacked at the start of each tick
// to form the full input.
//
// Examples:
//   SelfPerception        : own energy, velocity        (3 floats)
//   NearestFoodPerception : direction & distance        (3 floats)
//   TerrainPerception     : height, slope, etc.         (5 floats, M5)
//   SignalPerception      : neighborhood signal field   (K floats, F-COMM)
//
// The total input dimension of an agent is the sum of all attached
// perceptions' dim() values. This must match its brain's inputDim().

#include <span>

namespace neuro {

class IAgent;
class IWorld;

class IPerception {
public:
    virtual ~IPerception() = default;

    // How many floats this sensor writes per tick. Stable for the
    // perception's lifetime.
    virtual std::size_t dim() const = 0;

    // Fill out the next dim() elements of `out`. The caller guarantees
    // out.size() >= dim().
    virtual void sense(const IAgent& self,
                       const IWorld& world,
                       std::span<float> out) const = 0;

    // Identifier for logging / debugging.
    virtual const char* name() const = 0;
};

}  // namespace neuro
