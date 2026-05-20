#pragma once
//
// core/events/EventPayloads.h
//
// Strongly-typed payload structs for each EventType. Subscribers do
//   std::any_cast<const TickEnded&>(event.payload)
// after checking event.type.
//
// Keep payloads SMALL and PLAIN. They get copied into the deferred queue.
// Anything heavy (genomes, world snapshots) should be passed by id, not value.

#include "core/types/Aliases.h"
#include "core/types/Vec2.h"

#include <string>

namespace neuro::events {

// ---- M1 ----
struct TickStarted        { Tick tick; };
struct TickEnded          { Tick tick; };
struct GenerationStarted  { Generation gen; };
struct GenerationEnded    { Generation gen; double best_fitness; double avg_fitness; double min_fitness; };

struct AgentBorn          { AgentId id; GenomeId genome; Vec2 pos; };
struct AgentDied          { AgentId id; std::string cause; };
struct AgentMoved         { AgentId id; Vec2 from; Vec2 to; };
struct AgentAteFood       { AgentId id; Vec2 pos; float amount; };
struct AgentEnergyChanged { AgentId id; float old_energy; float new_energy; };

struct ResourceSpawned    { Vec2 pos; float amount; };
struct ResourceConsumed   { Vec2 pos; };

// ---- M2 ----
struct LanguageSwitched   { std::string language; };  // "zh" or "en"
struct UserPaused         {};
struct UserResumed        {};
struct UserSpeedChanged   { float multiplier; };      // 0.5 / 1.0 / 2.0 ...

// ---- M3 ----
struct SpeciesFormed      { SpeciesId id; SpeciesId parent; Generation gen; };
struct SpeciesExtinct     { SpeciesId id; Generation gen; };
struct MutationOccurred   { GenomeId genome; std::string kind; };

// ---- M4 ----
struct DecisionTriggered  { AgentId id; Tick tick; };

// ---- M5 ----
struct TerrainGenerated   { std::uint64_t seed; };
struct AgentEnteredCave   { AgentId id; Vec2 pos; };
struct AgentLeftCave      { AgentId id; Vec2 pos; };
struct TerrainModified    { AgentId id; Vec2 pos; float dig; float build; };

// ---- F-COMM ----
struct AgentEmittedSignal { AgentId id; Vec2 pos; std::uint8_t channel; float intensity; };

// ---- F-FIGHT ----
struct AgentAttacked      { AgentId attacker; AgentId victim; float damage; };
struct AgentFled          { AgentId id; Vec2 from; Vec2 to; float intensity; };

// ---- F-TOOL ----
struct ObjectPickedUp     { AgentId id; std::uint64_t object_id; };
struct ObjectDropped      { AgentId id; std::uint64_t object_id; };

// ---- F-MATE ----
struct MatingOccurred     { AgentId initiator; AgentId partner; };

// ---- F-GOD ----
struct InterventionTriggered { std::string kind; std::string user; };

// ---- F-FORK ----
struct UniverseForked     { WorldId source; WorldId fork; };
struct UniverseLoaded     { WorldId id; std::string snapshot_path; };

}  // namespace neuro::events
