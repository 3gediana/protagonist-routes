#pragma once
//
// core/events/EventBus.h
//
// Deferred-dispatch event bus. publish() pushes events to an internal
// queue; flush() actually invokes subscriber handlers. This decoupling
// avoids reentrancy bugs (subscribers publishing inside their handler).
//
// Threading model:
//   M0/M1:  single-threaded; mutex is mostly cheap insurance
//   M4+:    multi-threaded publish from worker threads; flush() must run
//           on the orchestration thread only.

#include "core/events/Event.h"
#include "core/events/EventType.h"
#include "core/interfaces/IEventBus.h"

#include <cstdint>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace neuro::events {

class EventBus : public IEventBus {
public:
    EventBus() = default;
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    EventBus(EventBus&&) = delete;
    EventBus& operator=(EventBus&&) = delete;
    ~EventBus() = default;

    // Subscribe to a specific event type. Returns a token used for
    // unsubscribe(). Tokens are unique across the bus.
    SubscriptionId subscribe(EventType type, Handler handler) override;

    // Subscribe to ALL events. Useful for loggers, recorders.
    SubscriptionId subscribeAny(Handler handler) override;

    // Stop receiving events. No-op if token was already removed.
    void unsubscribe(SubscriptionId token) override;

    // Enqueue an event. Cheap; only acquires the queue mutex briefly.
    void publish(Event event) override;

    // Construct-and-publish convenience.
    template <typename Payload>
    void publish(EventType type, Payload payload, Tick tick = 0) {
        publish(Event{type, std::move(payload), tick});
    }

    // Dispatch all queued events to subscribers, then clear the queue.
    // Subscribers calling publish() during dispatch will have their events
    // queued for the NEXT flush, not this one (avoids re-entry).
    void flush() override;

    // For testing / shutdown: drop everything queued.
    void clearQueue() override;

    // Diagnostics
    std::size_t pendingEventCount() const override;
    std::size_t subscriberCount() const override;

private:
    struct Subscription {
        SubscriptionId id;
        EventType      type;        // EventType::None for "any"
        Handler        handler;
    };

    mutable std::mutex            mutex_;
    std::vector<Subscription>     subscriptions_;
    std::vector<Event>            queue_;          // pending
    std::vector<Event>            dispatching_;    // double-buffer for flush
    SubscriptionId                next_id_{1};
};

}  // namespace neuro::events
