#pragma once

/**
 * ClockController.h — Simulation Clock Controller
 *
 * Reference: SDD Section 3.2
 *
 * The engine operates on a discrete clock tick model. The ClockController
 * drives tick advancement. Each tick represents one unit of simulated CPU time.
 *
 * Phase 1: Sequential single-threaded implementation (stub).
 * Phase 6: Full multi-threaded implementation with std::barrier.
 */

#include <vector>
#include <memory>
#include <string>

#include "core/ISimModule.h"

// Forward declarations
struct SimulationState;
class EventBus;

class ClockController {
public:
    /**
     * Construct the clock controller.
     * @param state  Reference to the shared simulation state
     * @param bus    Reference to the event bus
     */
    ClockController(SimulationState& state, EventBus& bus);

    /**
     * Register an OS module to be called on each tick.
     * Modules are called in the order they are registered.
     * @param module  Shared pointer to the module
     */
    void registerModule(std::shared_ptr<ISimModule> module);

    /**
     * Advance exactly one clock tick (Step Mode).
     * Calls onTick() on all registered modules sequentially,
     * then publishes a TICK_ADVANCED event.
     */
    void stepOnce();

    /**
     * Start the simulation clock.
     * In Phase 1 this just sets status to RUNNING.
     * In Phase 6 this will launch the auto-advance thread.
     */
    void start();

    /**
     * Pause the simulation clock without resetting state.
     */
    void pause();

    /**
     * Stop the clock and reset all state.
     */
    void reset();

    /**
     * Get the number of registered modules.
     */
    size_t getModuleCount() const;

private:
    SimulationState& state_;
    EventBus& bus_;
    std::vector<std::shared_ptr<ISimModule>> modules_;
};
