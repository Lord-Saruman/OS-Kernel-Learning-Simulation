/**
 * ClockController.cpp — Simulation Clock Controller Implementation
 *
 * Reference: SDD Section 3.2
 *
 * Phase 1 implementation: Sequential, single-threaded.
 * On each tick:
 *   1. Increment currentTick
 *   2. Call onTick() on each registered module (sequentially)
 *   3. Publish TICK_ADVANCED event
 *
 * Phase 6 will add:
 *   - std::barrier synchronisation
 *   - Per-module worker threads
 *   - Auto-advance timer thread
 *   - Condition variable for step mode signalling
 */

#include "core/ClockController.h"
#include "core/SimulationState.h"
#include "core/EventBus.h"
#include "core/SimEvent.h"

ClockController::ClockController(SimulationState& state, EventBus& bus)
    : state_(state)
    , bus_(bus)
    , modules_()
{}

void ClockController::registerModule(std::shared_ptr<ISimModule> module) {
    modules_.push_back(std::move(module));
}

void ClockController::stepOnce() {
    // Only advance if simulation is running
    if (state_.status != SimStatus::RUNNING) {
        return;
    }

    // Clear tick events from previous tick
    bus_.clearTickEvents();

    // Increment clock
    state_.currentTick++;

    // Call onTick() on each registered module sequentially
    // Phase 6 will parallelise this with std::barrier
    for (auto& module : modules_) {
        module->onTick(state_, bus_);
    }

    // Publish tick advancement event
    SimEvent tickEvent(
        state_.currentTick,
        EventTypes::TICK_ADVANCED,
        -1,   // system-level event, no source PID
        -1,   // no target PID
        -1,   // no resource ID
        "Clock tick " + std::to_string(state_.currentTick) + " completed"
    );
    bus_.publish(tickEvent);
}

void ClockController::start() {
    state_.status = SimStatus::RUNNING;
}

void ClockController::pause() {
    if (state_.status == SimStatus::RUNNING) {
        state_.status = SimStatus::PAUSED;
    }
}

void ClockController::reset() {
    state_.reset();
    bus_.reset();
    for (auto& module : modules_) {
        module->reset();
    }
}

size_t ClockController::getModuleCount() const {
    return modules_.size();
}
