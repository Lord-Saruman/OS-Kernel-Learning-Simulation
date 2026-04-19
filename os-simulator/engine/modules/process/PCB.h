#pragma once

/**
 * PCB.h — Process Control Block
 *
 * Reference: DataDictionary_MiniOS_Simulator.md, Section 3
 * Owned by:  Process Manager
 * Stored in: SimulationState::processTable
 *
 * The PCB is the central data structure of the simulator. It stores all
 * metadata the OS needs to manage a process throughout its lifecycle.
 * Every process has exactly one PCB. Created in NEW state, removed
 * after TERMINATED state metrics are collected.
 */

#include <string>
#include <vector>
#include <cstdint>

#include "core/SimEnums.h"

struct PCB {
    // ── Identity ─────────────────────────────────────────────
    int           pid;                // Unique process ID (>= 1, never reused)
    std::string   name;               // Human-readable name (max 32 chars, non-empty)
    ProcessType   type;               // CPU_BOUND | IO_BOUND | MIXED
    int           priority;           // 1 (highest) to 10 (lowest), default 5

    // ── State ────────────────────────────────────────────────
    ProcessState  state;              // Current lifecycle state
    uint64_t      arrivalTick;        // Tick when process entered NEW state
    uint64_t      startTick;          // Tick when first entered RUNNING (0 until then)
    uint64_t      terminationTick;    // Tick when reached TERMINATED (0 until then)

    // ── CPU Burst ────────────────────────────────────────────
    uint32_t      totalCpuBurst;      // Total CPU ticks required (>= 1)
    uint32_t      remainingBurst;     // Ticks remaining (0 triggers termination)
    uint32_t      quantumUsed;        // Ticks in current RR quantum (reset on dispatch)

    // ── I/O ──────────────────────────────────────────────────
    uint32_t      ioBurstDuration;    // Duration of single I/O burst (0 = no I/O)
    uint32_t      ioRemainingTicks;   // Ticks remaining in current I/O burst
    uint32_t      ioCompletionTick;   // Tick when current I/O completes (0 if not in I/O)

    // ── Memory ───────────────────────────────────────────────
    uint32_t      memoryRequirement;  // Number of virtual pages needed (>= 1)
    int           pageTableId;        // Key into SimulationState::pageTables (-1 if NEW)

    // ── Metrics (accumulated) ────────────────────────────────
    uint64_t      waitingTime;        // Accumulated ticks in READY state
    uint64_t      turnaroundTime;     // terminationTick - arrivalTick (0 until terminated)
    uint32_t      contextSwitches;    // Times preempted or descheduled
    uint32_t      pageFaultCount;     // Page faults during lifetime

    // ── Threads ──────────────────────────────────────────────
    std::vector<int> threadIds;       // TIDs belonging to this process

    // ── Default Constructor ──────────────────────────────────
    PCB()
        : pid(-1)
        , name("")
        , type(ProcessType::CPU_BOUND)
        , priority(5)
        , state(ProcessState::NEW)
        , arrivalTick(0)
        , startTick(0)
        , terminationTick(0)
        , totalCpuBurst(1)
        , remainingBurst(1)
        , quantumUsed(0)
        , ioBurstDuration(0)
        , ioRemainingTicks(0)
        , ioCompletionTick(0)
        , memoryRequirement(1)
        , pageTableId(-1)
        , waitingTime(0)
        , turnaroundTime(0)
        , contextSwitches(0)
        , pageFaultCount(0)
        , threadIds()
    {}
};
