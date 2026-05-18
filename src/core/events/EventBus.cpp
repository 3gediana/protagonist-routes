#include "core/events/EventBus.h"

#include <algorithm>
#include <utility>

namespace neuro::events {

SubscriptionId EventBus::subscribe(EventType type, Handler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    const SubscriptionId id = next_id_++;
    subscriptions_.push_back({id, type, std::move(handler)});
    return id;
}

SubscriptionId EventBus::subscribeAny(Handler handler) {
    return subscribe(EventType::None, std::move(handler));
}

void EventBus::unsubscribe(SubscriptionId token) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::remove_if(subscriptions_.begin(), subscriptions_.end(),
        [token](const Subscription& s) { return s.id == token; });
    subscriptions_.erase(it, subscriptions_.end());
}

void EventBus::publish(Event event) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push_back(std::move(event));
}

void EventBus::flush() {
    // Swap queue to local buffer under lock so dispatch happens lock-free.
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::swap(queue_, dispatching_);
    }

    if (dispatching_.empty()) return;

    // Take a snapshot of subscribers under lock too. This means subscribers
    // added DURING dispatch won't see this flush's events (consistent with
    // the deferred-dispatch contract).
    std::vector<Subscription> snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot = subscriptions_;
    }

    for (const Event& evt : dispatching_) {
        for (const Subscription& sub : snapshot) {
            if (sub.type == EventType::None || sub.type == evt.type()) {
                sub.handler(evt);
            }
        }
    }

    dispatching_.clear();
}

void EventBus::clearQueue() {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.clear();
    dispatching_.clear();
}

std::size_t EventBus::pendingEventCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

std::size_t EventBus::subscriberCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return subscriptions_.size();
}

}  // namespace neuro::events
