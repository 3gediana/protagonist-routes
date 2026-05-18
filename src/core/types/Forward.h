#pragma once
//
// core/types/Forward.h
//
// Forward declarations of all major core types. Include this header instead
// of dragging full headers when you only need a pointer / reference.
// Helps decouple compilation units and reduces rebuild times.

namespace neuro {

// ---- types ----
struct Vec2;

// ---- ids ----
namespace detail { template <typename Tag> struct StrongId; }
struct AgentTag;
struct GenomeTag;
struct SpeciesTag;
struct InnovationTag;
struct WorldTag;

// ---- core entities ----
class IBrain;
class IGenome;
class IWorld;
class IWorldLayer;
class IAgent;
class IPerception;
class IAgentCapability;
class IRewardComponent;
class IEvolutionStrategy;
class IObserver;
class IRenderer;
class IEventBus;

// ---- events ----
namespace events {
class Event;
class EventBus;
enum class EventType;
}  // namespace events

// ---- config ----
namespace config {
class Config;
}

// ---- logging ----
namespace log {
class Logger;
}

}  // namespace neuro
