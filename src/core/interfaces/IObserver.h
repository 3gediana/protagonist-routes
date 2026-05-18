#pragma once
//
// core/interfaces/IObserver.h
//
// A passive listener attached to the EventBus. Used for:
//   - Statistics collection (FitnessRecorder, SpeciesObserver)
//   - Logging (EventLogger)
//   - Replay recording (F-REPLAY)
//   - Heatmap accumulation (F-HEAT)
//   - LLM narration trigger (F-LLM)
//
// Observers are constructed by the runtime, given access to the world's
// EventBus, and detach themselves at shutdown.
//
// Contract: observers must NOT mutate world state. They are read-only
// listeners. If you need to act on events, that's a Layer or Capability.

#include "core/events/EventBus.h"

#include <vector>

namespace neuro {

class IWorld;

class IObserver {
public:
    virtual ~IObserver() = default;

    // Called once at startup. The observer subscribes to events here.
    // Implementations should store the returned SubscriptionId tokens
    // and unsubscribe in onDetach().
    virtual void onAttach(IWorld& world) = 0;

    // Called before shutdown. The observer unsubscribes here.
    virtual void onDetach(IWorld& world) = 0;

    // Identifier for logging.
    virtual const char* name() const = 0;
};

}  // namespace neuro
