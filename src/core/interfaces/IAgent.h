#pragma once
//
// core/interfaces/IAgent.h
//
// An individual living in the world. Owns a brain, has a position, holds
// per-individual state (energy, age). Composed at runtime by adding
// IPerception sources (read inputs from world) and IAgentCapability sinks
// (apply outputs to world).
//
// M1: Agent has fixed perceptions/capabilities.
// M3: SpeciesId field becomes meaningful.
// M4: energy / metabolism become the death criterion.
// M5: terrain perception appends inputs.

#include "core/types/Aliases.h"
#include "core/types/Vec2.h"

#include <memory>

namespace neuro {

class IBrain;

class IAgent {
public:
    virtual ~IAgent() = default;

    // Identity & lineage
    virtual AgentId   id()        const = 0;
    virtual GenomeId  genomeId()  const = 0;
    virtual SpeciesId speciesId() const = 0;       // kInvalidSpeciesId pre-M3

    // Position & velocity in continuous world space
    virtual Vec2      position()  const = 0;
    virtual Vec2      velocity()  const = 0;
    virtual void      setPosition(Vec2 p) = 0;
    virtual void      setVelocity(Vec2 v) = 0;

    // Resource state
    virtual float     energy()    const = 0;
    virtual void      setEnergy(float e) = 0;
    virtual Tick      age()       const = 0;       // ticks since birth

    // Status
    virtual bool      alive()     const = 0;
    virtual void      kill(const char* cause) = 0; // emits AgentDied event

    // Brain access (mutable, since forward() is non-const).
    virtual IBrain&        brain()       = 0;
    virtual const IBrain&  brain() const = 0;

    // Fitness so far this generation. Reset at generation boundary.
    virtual double    fitness()   const = 0;
    virtual void      addFitness(double delta) = 0;
    virtual void      resetFitness() = 0;
};

}  // namespace neuro
