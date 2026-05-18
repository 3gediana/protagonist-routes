#pragma once

#include "core/events/Event.h"

#include <cstddef>
#include <functional>

namespace neuro::events {

using SubscriptionId = std::uint64_t;
using Handler = std::function<void(const Event&)>;

class IEventBus {
public:
    virtual ~IEventBus() = default;

    virtual SubscriptionId subscribe(EventType type, Handler handler) = 0;
    virtual SubscriptionId subscribeAny(Handler handler) = 0;
    virtual void unsubscribe(SubscriptionId token) = 0;
    virtual void publish(Event event) = 0;
    virtual void flush() = 0;
    virtual void clearQueue() = 0;
    virtual std::size_t pendingEventCount() const = 0;
    virtual std::size_t subscriberCount() const = 0;
};

}  // namespace neuro::events
