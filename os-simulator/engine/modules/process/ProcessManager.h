#pragma once

/**
 * ProcessManager.h — Process Manager Module
 *
 * Reference: SDD Section 3.1 (Process Manager Module),
 *            PRD Section 6.1 (FR-PM-01 through FR-PM-05),
 *            DataDictionary Sections 3, 4, 8.2
 *
 * The Process Manager is the first OS subsystem module. It owns the
 * lifecycle of all processes and threads. Every other module depends
 * on processes existing in the system.
 *
 * Responsibilities:
 *   - Process creation (from ProcessSpec) and termination
 *   - Thread creation and termination
 *   - State machine transitions: NEW → READY, WAITING → READY, * → TERMINATED
 *   - I/O completion handling (decrement ioRemainingTicks, move to READY)
 *   - Waiting time accumulation for processes in READY state
 *   - Event publishing for all state changes
 *
 * Phase 2 boundary:
 *   - READY → RUNNING is owned by the Scheduler (Phase 3)
 *   - RUNNING → WAITING is triggered by the Scheduler (Phase 3)
 *   - This module handles NEW → READY, WAITING → READY, and * → TERMINATED
 */

#include <string>
#include <cstdint>

#include "core/ISimModule.h"
#include "core/SimEnums.h"
#include "modules/process/ProcessSpec.h"

// Forward declarations
struct SimulationState;
class EventBus;

class ProcessManager : public ISimModule {
public:
    ProcessManager();

    // ══════════════════════════════════════════════════════════
    // ISimModule interface
    // ══════════════════════════════════════════════════════════

    /**
     * Called once per clock tick by the ClockController.
     * Handles: admission (NEW→READY), I/O completions (WAITING→READY),
     * waiting time accumulation, terminated process retention.
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
    // Process Lifecycle API
    // (called by API Bridge in Phase 7, or directly in tests)
    // ══════════════════════════════════════════════════════════

    /**
     * Create a new process from a ProcessSpec.
     * The process is created in NEW state and will be admitted to
     * READY on the next onTick() call.
     *
     * @param state  Shared simulation state
     * @param bus    Event bus for publishing PROCESS_CREATED event
     * @param spec   Process specification (name, type, priority, bursts)
     * @return PID of the newly created process
     */
    int createProcess(SimulationState& state, EventBus& bus,
                      const ProcessSpec& spec);

    /**
     * Force-terminate a process. Moves it to TERMINATED state,
     * removes from readyQueue, and terminates all child threads.
     *
     * @param state  Shared simulation state
     * @param bus    Event bus for publishing PROCESS_TERMINATED event
     * @param pid    PID of the process to kill
     */
    void killProcess(SimulationState& state, EventBus& bus, int pid);

    // ══════════════════════════════════════════════════════════
    // Thread Lifecycle API
    // ══════════════════════════════════════════════════════════

    /**
     * Create a new thread within an existing process.
     *
     * @param state      Shared simulation state
     * @param bus        Event bus (reserved for future thread events)
     * @param parentPid  PID of the parent process
     * @param cpuBurst   CPU ticks this thread needs (default: 1)
     * @param stackSize  Simulated stack size in pages (default: 2)
     * @return TID of the newly created thread, or -1 on failure
     */
    int createThread(SimulationState& state, EventBus& bus,
                     int parentPid, uint32_t cpuBurst = 1,
                     uint32_t stackSize = 2);

private:
    ModuleStatus status_;

    // ══════════════════════════════════════════════════════════
    // Internal helpers — called by onTick()
    // ══════════════════════════════════════════════════════════

    /**
     * Admit all NEW processes to READY state and add to readyQueue.
     */
    void admitNewProcesses(SimulationState& state, EventBus& bus);

    /**
     * Decrement ioRemainingTicks for WAITING processes.
     * Move completed I/O processes back to READY.
     */
    void handleIOCompletions(SimulationState& state, EventBus& bus);

    /**
     * Increment waitingTime for all processes in READY state.
     */
    void updateWaitingTimes(SimulationState& state);

    // ══════════════════════════════════════════════════════════
    // Auto-assignment helpers (when ProcessSpec fields are 0)
    // ══════════════════════════════════════════════════════════

    uint32_t autoAssignBurst(ProcessType type);
    uint32_t autoAssignIO(ProcessType type);
    uint32_t autoAssignMemory(ProcessType type);
    std::string autoGenerateName(int pid);
};
