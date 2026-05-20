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

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

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

// ---- protagonist route (CT-2/CT-3 structured action events) ----
// Continuous-time semantics: a chop / build is not a single tick action but a
// sustained interaction with `started_at_seconds` -> `completed_at_seconds`.
// Memory + ledger layers consume these to reconstruct causal chains.
struct ChopStarted        { AgentId chopper; std::size_t tree_id; Vec2 tree_pos; double started_at_seconds; };
struct ChopCompleted      { AgentId chopper; std::size_t tree_id; Vec2 tree_pos; double started_at_seconds; double duration_seconds; std::size_t wood_yield; };
struct ChopAbandoned      { AgentId chopper; std::size_t tree_id; double started_at_seconds; double duration_seconds; float remaining_integrity; };
struct BuildStarted       { AgentId builder; std::size_t worksite_id; Vec2 site_pos; double started_at_seconds; };
// contributors[i] and contribution_seconds[i] form a 1-to-1 mapping sorted by
// descending contribution_seconds, so consumers can do share-of-total credit
// assignment without re-querying the worksite (which may already be reset).
struct BuildCompleted     { std::size_t worksite_id; Vec2 site_pos; std::vector<AgentId> contributors; std::vector<float> contribution_seconds; double started_at_seconds; double duration_seconds; };
struct WorksiteMaterialDeposited { AgentId depositor; std::size_t worksite_id; std::uint8_t material_type; double time_seconds; };
// CreditAssigned: emitted whenever the ContributionLedger pays out a share
// of a cooperative reward (Stage 2). source_kind = 0 -> worksite_build,
// 1 -> hunt_kill. source_id is the worksite id (for build) or victim id
// (for hunt). share is the agent's fraction of the legitimate contributors'
// total contribution. reward is the actual fitness delta in protagonist
// units (already weighted by cooperation_weight). Lets trace analyzers
// reconstruct who got credited for what without re-running the simulation.
struct CreditAssigned     { AgentId recipient; std::uint8_t source_kind; std::uint64_t source_id; float share; float reward; };

}  // namespace neuro::events
