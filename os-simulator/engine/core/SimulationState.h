#pragma once

/**
 * SimulationState.h — Central Shared Simulation State
 *
 * Reference: SDD Section 4.2, DataDictionary throughout
 *
 * The SimulationState object is the SINGLE SOURCE OF TRUTH for the
 * entire simulator. It is a C++ struct held in heap memory, accessed
 * by all modules via reference. All mutations are protected by a
 * std::shared_mutex (multiple readers, one writer at a time).
 *
 * The frontend is a read-only view of this state — it never holds
 * its own state independently.
 */

#include <map>
#include <deque>
#include <vector>
#include <string>
#include <cstdint>
#include <shared_mutex>

// Core types
#include "core/SimEnums.h"
#include "core/SimEvent.h"
#include "core/DecisionLogEntry.h"

// Data structures from each module
#include "modules/process/PCB.h"
#include "modules/process/TCB.h"
#include "modules/scheduler/GanttEntry.h"
#include "modules/scheduler/SchedulingMetrics.h"
#include "modules/sync/Mutex.h"
#include "modules/sync/Semaphore.h"
#include "modules/memory/PageTable.h"
#include "modules/memory/FrameTableEntry.h"
#include "modules/memory/MemoryMetrics.h"

struct SimulationState {
    // ══════════════════════════════════════════════════════════
    // Clock
    // ══════════════════════════════════════════════════════════
    uint64_t    currentTick;       // Current simulation clock tick
    SimMode     mode;              // STEP | AUTO
    SimStatus   status;            // IDLE | RUNNING | PAUSED | STOPPED
    uint32_t    autoSpeedMs;       // Tick interval in ms for AUTO mode

    // ══════════════════════════════════════════════════════════
    // Process Manager
    // ══════════════════════════════════════════════════════════
    std::map<int, PCB>    processTable;   // PID -> PCB
    std::map<int, TCB>    threadTable;    // TID -> TCB
    int                   nextPID;        // Next available PID (starts at 1)
    int                   nextTID;        // Next available TID (starts at 1)

    // ══════════════════════════════════════════════════════════
    // Scheduler
    // ══════════════════════════════════════════════════════════
    std::deque<int>           readyQueue;      // PIDs in ready queue
    int                       runningPID;      // PID of running process (-1 if idle)
    std::string               activePolicy;    // "FCFS" | "ROUND_ROBIN" | "PRIORITY_NP" | "PRIORITY_P"
    uint32_t                  timeQuantum;     // Time quantum for Round Robin
    std::vector<GanttEntry>   ganttLog;        // One entry per tick
    SchedulingMetrics         metrics;         // Aggregated scheduling metrics

    // ══════════════════════════════════════════════════════════
    // Sync Manager
    // ══════════════════════════════════════════════════════════
    std::map<int, Mutex>          mutexTable;      // mutexId -> Mutex
    std::map<int, Semaphore>      semaphoreTable;  // semId -> Semaphore
    std::map<int, std::deque<int>> blockedQueues;  // syncID -> blocked PIDs

    // ══════════════════════════════════════════════════════════
    // Memory Manager
    // ══════════════════════════════════════════════════════════
    std::vector<FrameTableEntry>  frameTable;         // Physical frames
    std::map<int, PageTable>      pageTables;         // PID -> PageTable
    std::string                   activeReplacement;  // "FIFO" | "LRU"
    MemoryMetrics                 memMetrics;         // Memory performance metrics

    // ══════════════════════════════════════════════════════════
    // Decision Log (for UI annotation)
    // ══════════════════════════════════════════════════════════
    std::vector<DecisionLogEntry> decisionLog;

    // ══════════════════════════════════════════════════════════
    // Synchronisation primitive (NOT serialised to JSON)
    // ══════════════════════════════════════════════════════════
    mutable std::shared_mutex stateMutex;

    // ══════════════════════════════════════════════════════════
    // Default Constructor — initialise to SDD/DD spec defaults
    // ══════════════════════════════════════════════════════════
    SimulationState()
        : currentTick(0)
        , mode(SimMode::STEP)
        , status(SimStatus::IDLE)
        , autoSpeedMs(500)
        , processTable()
        , threadTable()
        , nextPID(1)
        , nextTID(1)
        , readyQueue()
        , runningPID(-1)
        , activePolicy("FCFS")
        , timeQuantum(2)
        , ganttLog()
        , metrics()
        , mutexTable()
        , semaphoreTable()
        , blockedQueues()
        , frameTable()
        , pageTables()
        , activeReplacement("FIFO")
        , memMetrics()
        , decisionLog()
    {}

    /**
     * Reset all simulation state back to initial defaults.
     * Called on /sim/reset command.
     */
    void reset() {
        currentTick = 0;
        mode = SimMode::STEP;
        status = SimStatus::IDLE;
        autoSpeedMs = 500;

        processTable.clear();
        threadTable.clear();
        nextPID = 1;
        nextTID = 1;

        readyQueue.clear();
        runningPID = -1;
        activePolicy = "FCFS";
        timeQuantum = 2;
        ganttLog.clear();
        metrics = SchedulingMetrics();

        mutexTable.clear();
        semaphoreTable.clear();
        blockedQueues.clear();

        frameTable.clear();
        pageTables.clear();
        activeReplacement = "FIFO";
        memMetrics = MemoryMetrics();

        decisionLog.clear();
    }

    // Non-copyable due to std::shared_mutex
    SimulationState(const SimulationState&) = delete;
    SimulationState& operator=(const SimulationState&) = delete;

    // Movable
    SimulationState(SimulationState&&) = default;
    SimulationState& operator=(SimulationState&&) = default;
};
