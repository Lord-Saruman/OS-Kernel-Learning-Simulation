/**
 * EventBus.cpp — Implementation of the Synchronous Event Bus
 *
 * Reference: SDD Section 4.1
 */

#include "core/EventBus.h"

#include <algorithm>

int EventBus::subscribe(const std::string& eventType, EventHandler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    int id = nextSubId_++;
    subscribers_[eventType].emplace_back(id, std::move(handler));
    return id;
}

int EventBus::subscribeAll(EventHandler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    int id = nextSubId_++;
    globalSubscribers_.emplace_back(id, std::move(handler));
    return id;
}

void EventBus::unsubscribe(int subscriptionId) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Remove from type-specific subscribers
    for (auto& [type, subs] : subscribers_) {
        subs.erase(
            std::remove_if(subs.begin(), subs.end(),
                [subscriptionId](const auto& pair) {
                    return pair.first == subscriptionId;
                }),
            subs.end()
        );
    }

    // Remove from global subscribers
    globalSubscribers_.erase(
        std::remove_if(globalSubscribers_.begin(), globalSubscribers_.end(),
            [subscriptionId](const auto& pair) {
                return pair.first == subscriptionId;
            }),
        globalSubscribers_.end()
    );
}

void EventBus::publish(const SimEvent& event) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Store in tick events buffer
    tickEvents_.push_back(event);

    // Invoke type-specific handlers
    auto it = subscribers_.find(event.eventType);
    if (it != subscribers_.end()) {
        for (const auto& [id, handler] : it->second) {
            handler(event);
        }
    }

    // Invoke global handlers
    for (const auto& [id, handler] : globalSubscribers_) {
        handler(event);
    }
}

std::vector<SimEvent> EventBus::getTickEvents() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tickEvents_;
}

void EventBus::clearTickEvents() {
    std::lock_guard<std::mutex> lock(mutex_);
    tickEvents_.clear();
}

void EventBus::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    subscribers_.clear();
    globalSubscribers_.clear();
    tickEvents_.clear();
    nextSubId_ = 0;
}
