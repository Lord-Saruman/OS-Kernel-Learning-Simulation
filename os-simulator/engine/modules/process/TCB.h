#pragma once

/**
 * TCB.h — Thread Control Block
 *
 * Reference: DataDictionary_MiniOS_Simulator.md, Section 4
 * Owned by:  Process Manager
 * Stored in: SimulationState::threadTable
 *
 * Each thread within a process has a TCB. Lighter than a PCB — tracks
 * execution context within the process rather than resource ownership.
 * Threads share the memory space (and page table) of their parent process.
 */

#include <cstdint>

#include "core/SimEnums.h"

struct TCB {
    int           tid;                // Unique thread ID (>= 1, never reused)
    int           parentPid;          // PID of owning process (immutable)
    ThreadState   state;              // Current thread lifecycle state
    uint64_t      creationTick;       // Tick when thread was created
    uint32_t      stackSize;          // Simulated stack size in pages (>= 1, default 2)
    uint64_t      simulatedSP;        // Simulated stack pointer (visualisation only)
    uint32_t      cpuBurst;           // Total CPU ticks this thread needs (>= 1)
    uint32_t      remainingBurst;     // Remaining CPU ticks
    int           blockedOnSyncId;    // Sync primitive ID if blocked (-1 if not)
    uint64_t      waitingTime;        // Accumulated ticks in T_BLOCKED state

    // ── Default Constructor ──────────────────────────────────
    TCB()
        : tid(-1)
        , parentPid(-1)
        , state(ThreadState::T_NEW)
        , creationTick(0)
        , stackSize(2)
        , simulatedSP(0)
        , cpuBurst(1)
        , remainingBurst(1)
        , blockedOnSyncId(-1)
        , waitingTime(0)
    {}
};
