// tests/core/test_event_bus.cpp
#include "core/events/Event.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/events/EventType.h"

#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

using neuro::events::Event;
using neuro::events::EventBus;
using neuro::events::EventType;

TEST(EventBus, SubscribeAndReceive) {
    EventBus bus;
    int count = 0;
    bus.subscribe(EventType::TickStarted,
                  [&](const Event&) { ++count; });

    bus.publish(Event{EventType::TickStarted});
    EXPECT_EQ(count, 0) << "publish should be deferred";
    bus.flush();
    EXPECT_EQ(count, 1);
}

TEST(EventBus, MultipleSubscribersSameType) {
    EventBus bus;
    int a = 0, b = 0;
    bus.subscribe(EventType::AgentDied, [&](const Event&) { ++a; });
    bus.subscribe(EventType::AgentDied, [&](const Event&) { ++b; });

    bus.publish(Event{EventType::AgentDied});
    bus.publish(Event{EventType::AgentDied});
    bus.flush();
    EXPECT_EQ(a, 2);
    EXPECT_EQ(b, 2);
}

TEST(EventBus, FilterByType) {
    EventBus bus;
    int ticks = 0, deaths = 0;
    bus.subscribe(EventType::TickStarted, [&](const Event&) { ++ticks; });
    bus.subscribe(EventType::AgentDied,   [&](const Event&) { ++deaths; });

    bus.publish(Event{EventType::TickStarted});
    bus.publish(Event{EventType::AgentDied});
    bus.publish(Event{EventType::TickStarted});
    bus.flush();
    EXPECT_EQ(ticks, 2);
    EXPECT_EQ(deaths, 1);
}

TEST(EventBus, SubscribeAnyReceivesAll) {
    EventBus bus;
    int total = 0;
    bus.subscribeAny([&](const Event&) { ++total; });

    bus.publish(Event{EventType::TickStarted});
    bus.publish(Event{EventType::AgentDied});
    bus.publish(Event{EventType::GenerationEnded});
    bus.flush();
    EXPECT_EQ(total, 3);
}

TEST(EventBus, Unsubscribe) {
    EventBus bus;
    int count = 0;
    auto token = bus.subscribe(EventType::TickStarted,
                               [&](const Event&) { ++count; });
    bus.publish(Event{EventType::TickStarted});
    bus.flush();
    EXPECT_EQ(count, 1);

    bus.unsubscribe(token);
    bus.publish(Event{EventType::TickStarted});
    bus.flush();
    EXPECT_EQ(count, 1) << "unsubscribed handler must not be called";
}

TEST(EventBus, PayloadAccess) {
    EventBus bus;
    int agents_died = 0;
    std::string captured_cause;

    bus.subscribe(EventType::AgentDied, [&](const Event& e) {
        const auto& payload = e.payloadAs<neuro::events::AgentDied>();
        ++agents_died;
        captured_cause = payload.cause;
    });

    bus.publish(EventType::AgentDied,
                neuro::events::AgentDied{neuro::AgentId{42}, "starvation"});
    bus.flush();

    EXPECT_EQ(agents_died, 1);
    EXPECT_EQ(captured_cause, "starvation");
}

TEST(EventBus, PublishDuringFlushDoesNotReenter) {
    // A subscriber that publishes another event must NOT cause that event to
    // be dispatched in the same flush; it should land in the next flush.
    EventBus bus;
    int first_calls = 0;
    int second_calls = 0;

    bus.subscribe(EventType::TickStarted, [&](const Event&) {
        ++first_calls;
        bus.publish(Event{EventType::TickEnded});
    });
    bus.subscribe(EventType::TickEnded, [&](const Event&) {
        ++second_calls;
    });

    bus.publish(Event{EventType::TickStarted});
    bus.flush();
    EXPECT_EQ(first_calls, 1);
    EXPECT_EQ(second_calls, 0) << "TickEnded should not fire in same flush";

    bus.flush();
    EXPECT_EQ(second_calls, 1) << "TickEnded should fire on next flush";
}

TEST(EventBus, ThreadSafePublish) {
    // Multiple threads publishing concurrently; a single thread flushing.
    // We expect no crashes / no lost events.
    EventBus bus;
    std::atomic<int> received{0};
    bus.subscribe(EventType::TickStarted,
                  [&](const Event&) { received.fetch_add(1); });

    constexpr int kThreads = 4;
    constexpr int kPerThread = 100;
    std::vector<std::thread> workers;
    workers.reserve(kThreads);
    for (int i = 0; i < kThreads; ++i) {
        workers.emplace_back([&] {
            for (int j = 0; j < kPerThread; ++j) {
                bus.publish(Event{EventType::TickStarted});
            }
        });
    }
    for (auto& w : workers) w.join();

    bus.flush();
    EXPECT_EQ(received.load(), kThreads * kPerThread);
}

TEST(EventBus, ToStringCoversAllTypes) {
    // Spot-check that toString does not return "Unknown" for known types.
    using neuro::events::toString;
    EXPECT_STRNE(toString(EventType::TickStarted), "Unknown");
    EXPECT_STRNE(toString(EventType::AgentDied), "Unknown");
    EXPECT_STRNE(toString(EventType::SpeciesFormed), "Unknown");
}
