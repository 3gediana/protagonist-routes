#pragma once
//
// core/events/EventType.h
//
// Enumeration of all event types in the system. Adding a new event:
//   1. Add a value here, grouped with related events
//   2. Add a payload struct in EventPayloads.h
//   3. Document in this file's comment block
//
// Keep this list SORTED BY PHASE so future agents can see at a glance
// which events belong to which baseline / module.

#include <cstdint>

namespace neuro::events {

enum class EventType : std::uint16_t {
    // ====================== M0 / infrastructure ===========================
    None = 0,
    Shutdown,                   // runtime exiting

    // ====================== M1 / world & evolution =======================
    TickStarted,                // one simulation tick is about to run
    TickEnded,                  // tick complete
    GenerationStarted,          // new generation begin
    GenerationEnded,            // generation complete (best evaluated)

    AgentBorn,                  // new agent appended to population
    AgentDied,                  // agent removed (energy depleted / killed)
    AgentMoved,                 // agent's position changed (may be throttled)
    AgentAteFood,               // agent consumed food
    AgentEnergyChanged,         // any change to agent's energy

    ResourceSpawned,            // food appeared
    ResourceConsumed,           // food disappeared

    // ====================== M2 / visualization ===========================
    LanguageSwitched,           // user toggled UI language
    UserPaused,
    UserResumed,
    UserSpeedChanged,

    // ====================== M3 / NEAT ====================================
    SpeciesFormed,              // a new NEAT species was created
    SpeciesExtinct,             // a species lost all members
    MutationOccurred,           // a structural mutation happened (debug)

    // ====================== M4 / async ===================================
    DecisionTriggered,          // a brain ran inference

    // ====================== M5 / terrain =================================
    TerrainGenerated,           // a fresh terrain instance ready
    AgentEnteredCave,
    AgentLeftCave,
    TerrainModified,

    // ====================== F-COMM ========================================
    AgentEmittedSignal,

    // ====================== F-FIGHT =======================================
    AgentAttacked,
    AgentFled,
    ObjectPickedUp,
    ObjectDropped,

    // ====================== F-MATE ========================================
    MatingOccurred,             // two agents mated (spatial partner selection)

    // ====================== F-GOD =========================================
    InterventionTriggered,

    // ====================== F-FORK ========================================
    UniverseForked,
    UniverseLoaded,

    // ====================== sentinel ======================================
    Count,                      // keep last
};

// Helper: stringify for logging / debugging. Implementation in
// EventType.cpp keeps the switch in one place.
const char* toString(EventType type);

}  // namespace neuro::events
