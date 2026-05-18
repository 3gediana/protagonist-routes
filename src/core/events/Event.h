#pragma once
//
// core/events/Event.h
//
// A single event flowing through the EventBus. Holds a type tag and an
// opaque payload accessible via std::any_cast.

#include "core/events/EventType.h"
#include "core/types/Aliases.h"

#include <any>
#include <utility>

namespace neuro::events {

class Event {
public:
    Event() = default;

    template <typename Payload>
    Event(EventType type, Payload payload, Tick tick = 0)
        : type_(type), payload_(std::move(payload)), tick_(tick) {}

    Event(EventType type, Tick tick = 0) : type_(type), tick_(tick) {}

    EventType   type()    const noexcept { return type_; }
    Tick        tick()    const noexcept { return tick_; }
    const std::any& payload() const noexcept { return payload_; }

    // Type-safe payload access. Returns nullptr if cast fails.
    template <typename Payload>
    const Payload* tryPayload() const noexcept {
        return std::any_cast<Payload>(&payload_);
    }

    // Throws std::bad_any_cast if the payload type doesn't match.
    template <typename Payload>
    const Payload& payloadAs() const {
        return std::any_cast<const Payload&>(payload_);
    }

private:
    EventType type_{EventType::None};
    std::any  payload_{};
    Tick      tick_{0};
};

}  // namespace neuro::events
