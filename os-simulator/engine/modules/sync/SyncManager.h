#pragma once

/**
 * SyncManager.h — Sync Manager Module
 *
 * Reference: SDD Section 3.1 (Sync Manager Module),
 *            PRD Section 6.3 (FR-SY-01 through FR-SY-05),
 *            DataDictionary Sections 2.8, 6.1, 6.2
 *
 * The Sync Manager is the fourth OS subsystem module. It manages
 * mutex locks and semaphore primitives, maintaining blocked queues
 * and ownership tracking. It includes a built-in race condition
 * demonstration scenario.
 *
 * Responsibilities:
 *   - Mutex creation with ownership tracking
 *   - Binary and counting semaphore creation
 *   - Process blocking (RUNNING/READY → WAITING) on lock contention
 *   - Process unblocking (WAITING → READY) on lock release/signal
 *   - Blocked queue management (per-primitive FIFO queues)
 *   - Auto-release of locks held by terminated processes
 *   - Race condition demonstration (with and without synchronization)
 *   - Event publishing for all sync operations
 *
 * Tick execution order (SDD §3.2):
 *   Process Manager and Scheduler run BEFORE the Sync Manager.
 *   The Sync Manager processes queued sync requests and manages
 *   process blocking/unblocking based on primitive state.
 *
 * DEFERRED (Phase 5 → Future):
 *   - Thread-level sync (TCB.blockedOnSyncId) — see implementation_plan.md
 *   - REST endpoints for sync operations — deferred to Phase 7
 *   - Race condition demo as workload scenario — deferred to Phase 7
 */

#include <string>
#include <vector>
#include <cstdint>

#include "core/ISimModule.h"
#include "core/SimEnums.h"
#include "modules/sync/SyncRequest.h"

// Forward declarations
struct SimulationState;
class EventBus;

class SyncManager : public ISimModule {
public:
    SyncManager();

    // ══════════════════════════════════════════════════════════
    // ISimModule interface
    // ══════════════════════════════════════════════════════════

    /**
     * Called once per clock tick by the ClockController.
     *
     * Sequence:
     *   1. processSyncRequests     — drain pendingSyncRequests queue
     *   2. cleanupTerminatedProcessLocks — auto-release dead owners
     *   3. updateBlockedQueues     — rebuild blockedQueues map
     */
    void onTick(SimulationState& state, EventBus& bus) override;

    /**
     * Reset module to initial state. Called on simulation reset.
     */
    void reset() override;

    /**
     * Get the current operational status of this module.
     */
    ModuleStatus getStatus() const override;

    /**
     * Get the human-readable name of this module.
     */
    std::string getModuleName() const override;

    // ══════════════════════════════════════════════════════════
    // Primitive Creation API
    // (called by API Bridge in Phase 7, or directly in tests)
    // ══════════════════════════════════════════════════════════

    /**
     * Create a new mutex.
     *
     * @param state  Shared simulation state
     * @param bus    Event bus for publishing events
     * @param name   Human-readable label for the mutex (max 32 chars)
     * @return mutexId of the newly created mutex
     */
    int createMutex(SimulationState& state, EventBus& bus,
                    const std::string& name);

    /**
     * Create a new semaphore.
     *
     * @param state        Shared simulation state
     * @param bus          Event bus for publishing events
     * @param name         Human-readable label (max 32 chars)
     * @param type         SEMAPHORE_BINARY or SEMAPHORE_COUNTING
     * @param initialValue Initial count (1 for binary, >= 1 for counting)
     * @return semId of the newly created semaphore
     */
    int createSemaphore(SimulationState& state, EventBus& bus,
                        const std::string& name,
                        SyncPrimitiveType type,
                        int initialValue);

    // ══════════════════════════════════════════════════════════
    // Sync Request API
    // Enqueues sync operations into pendingSyncRequests.
    // Processed on the next onTick() call.
    // ══════════════════════════════════════════════════════════

    /**
     * Request mutex acquire for a process.
     */
    void requestAcquire(SimulationState& state, int pid, int mutexId);

    /**
     * Request mutex release for a process.
     */
    void requestRelease(SimulationState& state, int pid, int mutexId);

    /**
     * Request semaphore wait (P / down) for a process.
     */
    void requestWait(SimulationState& state, int pid, int semId);

    /**
     * Request semaphore signal (V / up) for a process.
     */
    void requestSignal(SimulationState& state, int pid, int semId);

    // ══════════════════════════════════════════════════════════
    // Race Condition Demo API (FR-SY-03, FR-SY-04)
    // ══════════════════════════════════════════════════════════

    /**
     * Set up the race condition demonstration scenario.
     * Creates 2 writer processes and 1 mutex. Tracks a shared counter
     * to demonstrate data corruption without sync vs. correct behavior
     * with sync enabled.
     *
     * @param state  Shared simulation state
     * @param bus    Event bus
     * @param useSynchronization  true = protected by mutex, false = unprotected
     */
    void setupRaceConditionDemo(SimulationState& state, EventBus& bus,
                                bool useSynchronization);

    /**
     * Check if the race condition demo is currently active.
     */
    bool isRaceConditionDemoActive() const;

    /**
     * Get the current shared counter value (race condition demo).
     */
    int getRaceConditionCounter() const;

    /**
     * Get the race condition access log.
     */
    const std::vector<std::string>& getRaceConditionLog() const;

    /**
     * Get the expected correct counter value (race condition demo).
     */
    int getRaceConditionExpectedValue() const;

private:
    ModuleStatus status_;
    int nextMutexId_;                    // Auto-incrementing mutex ID
    int nextSemId_;                      // Auto-incrementing semaphore ID

    // Race condition demo state
    bool raceConditionDemoActive_;
    bool raceConditionUsesSync_;         // Whether the demo uses mutex protection
    int  raceConditionMutexId_;          // Mutex used for synchronized demo
    int  raceConditionPid1_;             // PID of first writer process
    int  raceConditionPid2_;             // PID of second writer process
    int  sharedCounter_;                 // The shared resource being "written"
    int  expectedCounter_;               // What the counter should be if correct
    std::vector<std::string> raceConditionLog_;  // Access log for visualization

    // ══════════════════════════════════════════════════════════
    // Per-tick steps — called in order by onTick()
    // ══════════════════════════════════════════════════════════

    /**
     * Drain all pending sync requests and route to handlers.
     */
    void processSyncRequests(SimulationState& state, EventBus& bus);

    /**
     * Handle mutex acquire request.
     */
    void handleAcquire(SimulationState& state, EventBus& bus,
                       const SyncRequest& req);

    /**
     * Handle mutex release request.
     */
    void handleRelease(SimulationState& state, EventBus& bus,
                       const SyncRequest& req);

    /**
     * Handle semaphore wait (P) request.
     */
    void handleWait(SimulationState& state, EventBus& bus,
                    const SyncRequest& req);

    /**
     * Handle semaphore signal (V) request.
     */
    void handleSignal(SimulationState& state, EventBus& bus,
                      const SyncRequest& req);

    /**
     * Auto-release locks held by terminated processes.
     * Remove terminated processes from all waiting queues.
     */
    void cleanupTerminatedProcessLocks(SimulationState& state, EventBus& bus);

    /**
     * Rebuild state.blockedQueues from mutex and semaphore waiting lists.
     */
    void updateBlockedQueues(SimulationState& state);

    /**
     * Process race condition demo logic for the current tick.
     * Injects sync requests for the demo processes.
     */
    void tickRaceConditionDemo(SimulationState& state, EventBus& bus);

    // ══════════════════════════════════════════════════════════
    // Helpers
    // ══════════════════════════════════════════════════════════

    /**
     * Block a process: set state to WAITING, remove from readyQueue,
     * clear runningPID if needed.
     */
    void blockProcess(SimulationState& state, int pid);

    /**
     * Unblock a process: set state to READY, add to readyQueue.
     */
    void unblockProcess(SimulationState& state, int pid);
};
