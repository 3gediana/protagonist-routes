#pragma once
//
// core/interfaces/IGenome.h
//
// Opaque genetic blueprint. Concrete subclasses (FixedTopologyGenome in M1,
// NeatGenome in M3) define their own structure. The interface only requires
// what's needed for evolution and reproduction.
//
// Why "opaque"? The evolution layer should NOT need to know whether a
// genome is fixed-topology weights or NEAT graph structure. Subclasses
// expose their internals to their own brain builder via downcast.

#include "core/types/Aliases.h"

#include <memory>

namespace neuro {

class IGenome {
public:
    virtual ~IGenome() = default;

    // Unique identity for this genome (assigned at creation by the
    // population). Must remain stable across the genome's lifetime.
    virtual GenomeId id() const = 0;

    // Deep copy. Returned genome has a NEW id (caller is responsible for
    // assigning); used during reproduction.
    virtual std::unique_ptr<IGenome> clone() const = 0;

    // Coarse topological complexity. Used for stats and tournament tiebreaks.
    // Fixed-topology genomes return their parameter count; NEAT returns
    // node + connection count.
    virtual std::size_t complexity() const = 0;
};

}  // namespace neuro
