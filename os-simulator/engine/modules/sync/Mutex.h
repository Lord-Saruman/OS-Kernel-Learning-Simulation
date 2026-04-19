#pragma once

/**
 * Mutex.h — Simulated Mutex Lock
 *
 * Reference: DataDictionary_MiniOS_Simulator.md, Section 6.1
 * Stored in: SimulationState::mutexTable
 *
 * A simulated mutex providing mutual exclusion. Exactly one process
 * may hold a mutex at a time. Others attempting acquire() are moved
 * to WAITING and placed in the blocked queue.
 */

#include <string>
#include <deque>
#include <cstdint>

struct Mutex {
    int              mutexId;           // Unique ID (>= 1)
    std::string      name;              // Human-readable label (max 32 chars)
    bool             locked;            // true if currently held
    int              ownerPid;          // PID of holder (-1 if unlocked)
    std::deque<int>  waitingPids;       // FIFO queue of blocked PIDs
    uint64_t         lockedAtTick;      // Tick when last acquired (0 if unlocked)
    uint32_t         totalAcquisitions; // Cumulative successful lock count
    uint32_t         totalContentions;  // Times a process was blocked (contention)

    // ── Default Constructor ──────────────────────────────────
    Mutex()
        : mutexId(-1)
        , name("")
        , locked(false)
        , ownerPid(-1)
        , waitingPids()
        , lockedAtTick(0)
        , totalAcquisitions(0)
        , totalContentions(0)
    {}
};
