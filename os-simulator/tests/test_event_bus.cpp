/**
 * test_event_bus.cpp — Unit Tests for EventBus
 *
 * Verifies the pub/sub mechanism: subscription, publishing, filtering,
 * unsubscription, tick event buffering, and reset.
 */

#include <gtest/gtest.h>

#include "core/EventBus.h"
#include "core/SimEvent.h"

// ── Helper: create a SimEvent ────────────────────────────────
SimEvent makeEvent(const std::string& type, int srcPid = -1, const std::string& desc = "test") {
    return SimEvent(1, type, srcPid, -1, -1, desc);
}

// ══════════════════════════════════════════════════════════════
// Subscribe and Publish
// ══════════════════════════════════════════════════════════════

TEST(EventBusTest, SubscribeAndPublish) {
    EventBus bus;
    bool called = false;
    std::string receivedType;

    bus.subscribe(EventTypes::PROCESS_CREATED, [&](const SimEvent& e) {
        called = true;
        receivedType = e.eventType;
    });

    bus.publish(makeEvent(EventTypes::PROCESS_CREATED));

    EXPECT_TRUE(called);
    EXPECT_EQ(receivedType, EventTypes::PROCESS_CREATED);
}

TEST(EventBusTest, SubscriberNotCalledForDifferentType) {
    EventBus bus;
    bool called = false;

    bus.subscribe(EventTypes::PROCESS_CREATED, [&](const SimEvent&) {
        called = true;
    });

    // Publish a different event type
    bus.publish(makeEvent(EventTypes::PAGE_FAULT));

    EXPECT_FALSE(called);
}

// ══════════════════════════════════════════════════════════════
// Multiple Subscribers
// ══════════════════════════════════════════════════════════════

TEST(EventBusTest, MultipleSubscribersSameType) {
    EventBus bus;
    int callCount = 0;

    bus.subscribe(EventTypes::CPU_SCHEDULED, [&](const SimEvent&) { callCount++; });
    bus.subscribe(EventTypes::CPU_SCHEDULED, [&](const SimEvent&) { callCount++; });
    bus.subscribe(EventTypes::CPU_SCHEDULED, [&](const SimEvent&) { callCount++; });

    bus.publish(makeEvent(EventTypes::CPU_SCHEDULED));

    EXPECT_EQ(callCount, 3);
}

TEST(EventBusTest, SubscribersDifferentTypes) {
    EventBus bus;
    bool createdCalled = false;
    bool faultCalled = false;

    bus.subscribe(EventTypes::PROCESS_CREATED, [&](const SimEvent&) { createdCalled = true; });
    bus.subscribe(EventTypes::PAGE_FAULT, [&](const SimEvent&) { faultCalled = true; });

    bus.publish(makeEvent(EventTypes::PROCESS_CREATED));

    EXPECT_TRUE(createdCalled);
    EXPECT_FALSE(faultCalled);
}

// ══════════════════════════════════════════════════════════════
// Global Subscriber (subscribeAll)
// ══════════════════════════════════════════════════════════════

TEST(EventBusTest, GlobalSubscriberReceivesAllTypes) {
    EventBus bus;
    int callCount = 0;

    bus.subscribeAll([&](const SimEvent&) { callCount++; });

    bus.publish(makeEvent(EventTypes::PROCESS_CREATED));
    bus.publish(makeEvent(EventTypes::PAGE_FAULT));
    bus.publish(makeEvent(EventTypes::LOCK_ACQUIRED));
    bus.publish(makeEvent(EventTypes::TICK_ADVANCED));

    EXPECT_EQ(callCount, 4);
}

TEST(EventBusTest, GlobalAndTypeSpecificBothCalled) {
    EventBus bus;
    bool globalCalled = false;
    bool specificCalled = false;

    bus.subscribeAll([&](const SimEvent&) { globalCalled = true; });
    bus.subscribe(EventTypes::CONTEXT_SWITCH, [&](const SimEvent&) { specificCalled = true; });

    bus.publish(makeEvent(EventTypes::CONTEXT_SWITCH));

    EXPECT_TRUE(globalCalled);
    EXPECT_TRUE(specificCalled);
}

// ══════════════════════════════════════════════════════════════
// Unsubscribe
// ══════════════════════════════════════════════════════════════

TEST(EventBusTest, UnsubscribeStopsCalls) {
    EventBus bus;
    int callCount = 0;

    int subId = bus.subscribe(EventTypes::LOCK_RELEASED, [&](const SimEvent&) {
        callCount++;
    });

    bus.publish(makeEvent(EventTypes::LOCK_RELEASED));
    EXPECT_EQ(callCount, 1);

    bus.unsubscribe(subId);

    bus.publish(makeEvent(EventTypes::LOCK_RELEASED));
    EXPECT_EQ(callCount, 1);  // Should NOT have been called again
}

TEST(EventBusTest, UnsubscribeGlobalSubscriber) {
    EventBus bus;
    int callCount = 0;

    int subId = bus.subscribeAll([&](const SimEvent&) { callCount++; });

    bus.publish(makeEvent(EventTypes::TICK_ADVANCED));
    EXPECT_EQ(callCount, 1);

    bus.unsubscribe(subId);

    bus.publish(makeEvent(EventTypes::TICK_ADVANCED));
    EXPECT_EQ(callCount, 1);
}

// ══════════════════════════════════════════════════════════════
// Tick Events Buffer
// ══════════════════════════════════════════════════════════════

TEST(EventBusTest, TickEventsCollected) {
    EventBus bus;

    bus.publish(makeEvent(EventTypes::PROCESS_CREATED, 1, "Created P1"));
    bus.publish(makeEvent(EventTypes::CPU_SCHEDULED, 1, "Scheduled P1"));
    bus.publish(makeEvent(EventTypes::PAGE_FAULT, 1, "Page fault on P1"));

    auto events = bus.getTickEvents();
    EXPECT_EQ(events.size(), 3u);
    EXPECT_EQ(events[0].eventType, EventTypes::PROCESS_CREATED);
    EXPECT_EQ(events[1].eventType, EventTypes::CPU_SCHEDULED);
    EXPECT_EQ(events[2].eventType, EventTypes::PAGE_FAULT);
}

TEST(EventBusTest, ClearTickEventsWorks) {
    EventBus bus;

    bus.publish(makeEvent(EventTypes::PROCESS_CREATED));
    bus.publish(makeEvent(EventTypes::PAGE_FAULT));
    EXPECT_EQ(bus.getTickEvents().size(), 2u);

    bus.clearTickEvents();
    EXPECT_EQ(bus.getTickEvents().size(), 0u);
}

TEST(EventBusTest, ClearDoesNotAffectSubscriptions) {
    EventBus bus;
    int callCount = 0;

    bus.subscribe(EventTypes::TICK_ADVANCED, [&](const SimEvent&) { callCount++; });

    bus.publish(makeEvent(EventTypes::TICK_ADVANCED));
    bus.clearTickEvents();

    // Subscription still active — should still be called
    bus.publish(makeEvent(EventTypes::TICK_ADVANCED));
    EXPECT_EQ(callCount, 2);
}

// ══════════════════════════════════════════════════════════════
// Reset
// ══════════════════════════════════════════════════════════════

TEST(EventBusTest, ResetClearsEverything) {
    EventBus bus;
    int callCount = 0;

    bus.subscribe(EventTypes::PROCESS_CREATED, [&](const SimEvent&) { callCount++; });
    bus.subscribeAll([&](const SimEvent&) { callCount++; });
    bus.publish(makeEvent(EventTypes::PROCESS_CREATED));

    EXPECT_EQ(callCount, 2);  // Both handlers called
    EXPECT_EQ(bus.getTickEvents().size(), 1u);

    bus.reset();

    // After reset: no subscribers, no events
    EXPECT_EQ(bus.getTickEvents().size(), 0u);

    bus.publish(makeEvent(EventTypes::PROCESS_CREATED));
    EXPECT_EQ(callCount, 2);  // Should NOT have changed — handlers removed
}

// ══════════════════════════════════════════════════════════════
// Event Data Integrity
// ══════════════════════════════════════════════════════════════

TEST(EventBusTest, EventDataPassedCorrectly) {
    EventBus bus;
    SimEvent received;

    bus.subscribe(EventTypes::PROCESS_STATE_CHANGED, [&](const SimEvent& e) {
        received = e;
    });

    SimEvent original(42, EventTypes::PROCESS_STATE_CHANGED, 3, 5, 7, "P3 moved to READY");
    bus.publish(original);

    EXPECT_EQ(received.tick, 42u);
    EXPECT_EQ(received.eventType, EventTypes::PROCESS_STATE_CHANGED);
    EXPECT_EQ(received.sourcePid, 3);
    EXPECT_EQ(received.targetPid, 5);
    EXPECT_EQ(received.resourceId, 7);
    EXPECT_EQ(received.description, "P3 moved to READY");
}

// ══════════════════════════════════════════════════════════════
// All Event Types Defined
// ══════════════════════════════════════════════════════════════

TEST(EventTypesTest, AllTypesDefined) {
    // Verify all 12 event type constants exist and are non-empty
    EXPECT_NE(std::string(EventTypes::PROCESS_CREATED), "");
    EXPECT_NE(std::string(EventTypes::PROCESS_STATE_CHANGED), "");
    EXPECT_NE(std::string(EventTypes::PROCESS_TERMINATED), "");
    EXPECT_NE(std::string(EventTypes::CPU_SCHEDULED), "");
    EXPECT_NE(std::string(EventTypes::CONTEXT_SWITCH), "");
    EXPECT_NE(std::string(EventTypes::LOCK_ACQUIRED), "");
    EXPECT_NE(std::string(EventTypes::LOCK_RELEASED), "");
    EXPECT_NE(std::string(EventTypes::PROCESS_BLOCKED), "");
    EXPECT_NE(std::string(EventTypes::PROCESS_UNBLOCKED), "");
    EXPECT_NE(std::string(EventTypes::PAGE_FAULT), "");
    EXPECT_NE(std::string(EventTypes::PAGE_REPLACED), "");
    EXPECT_NE(std::string(EventTypes::TICK_ADVANCED), "");
}
