#pragma once

/**
 * EventBus.h — Synchronous Publish-Subscribe Event Bus
 *
 * Reference: SDD Section 4.1
 *
 * The Event Bus is the ONLY communication channel between modules and
 * the outside world. Modules never know who (if anyone) is listening
 * to their events. It is a synchronous pub-sub mechanism — publish()
 * invokes all matching handlers immediately in the caller's thread.
 *
 * Supported event types (from SDD §4.1):
 *   PROCESS_CREATED, PROCESS_STATE_CHANGED, PROCESS_TERMINATED,
 *   CPU_SCHEDULED, CONTEXT_SWITCH, LOCK_ACQUIRED, LOCK_RELEASED,
 *   PROCESS_BLOCKED, PROCESS_UNBLOCKED, PAGE_FAULT, PAGE_REPLACED,
 *   TICK_ADVANCED
 */

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <mutex>
#include <utility>

#include "core/SimEvent.h"

class EventBus {
public:
    using EventHandler = std::function<void(const SimEvent&)>;

    EventBus() : nextSubId_(0) {}

    /**
     * Subscribe to a specific event type.
     * @param eventType  Event type string (use EventTypes:: constants)
     * @param handler    Callback invoked when matching event is published
     * @return Subscription ID (for unsubscribe)
     */
    int subscribe(const std::string& eventType, EventHandler handler);

    /**
     * Subscribe to ALL event types. Used primarily by the API Bridge.
     * @param handler  Callback invoked on every event
     * @return Subscription ID
     */
    int subscribeAll(EventHandler handler);

    /**
     * Remove a subscription by ID.
     * @param subscriptionId  ID returned by subscribe/subscribeAll
     */
    void unsubscribe(int subscriptionId);

    /**
     * Publish an event. Invokes all matching handlers synchronously.
     * Also stores the event in the tick events buffer.
     * @param event  The event to publish
     */
    void publish(const SimEvent& event);

    /**
     * Get all events published during the current tick.
     * Used by the API Bridge for WebSocket broadcast.
     */
    std::vector<SimEvent> getTickEvents() const;

    /**
     * Clear the tick events buffer. Called at start of each new tick.
     */
    void clearTickEvents();

    /**
     * Reset all subscriptions and events. Called on simulation reset.
     */
    void reset();

private:
    mutable std::recursive_mutex mutex_;

    // Type-specific subscribers: eventType -> [(subId, handler)]
    std::map<std::string, std::vector<std::pair<int, EventHandler>>> subscribers_;

    // Global subscribers (receive ALL events): [(subId, handler)]
    std::vector<std::pair<int, EventHandler>> globalSubscribers_;

    // Events published during the current tick
    std::vector<SimEvent> tickEvents_;

    // Auto-incrementing subscription ID counter
    int nextSubId_;
};
