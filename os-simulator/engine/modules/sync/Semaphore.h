#pragma once

/**
 * Semaphore.h — Simulated Counting Semaphore
 *
 * Reference: DataDictionary_MiniOS_Simulator.md, Section 6.2
 * Stored in: SimulationState::semaphoreTable
 *
 * wait() decrements value; if it would go below 0, the process is blocked.
 * signal() increments value and unblocks one waiting process if any.
 */

#include <string>
#include <deque>
#include <cstdint>

#include "core/SimEnums.h"

struct Semaphore {
    int               semId;           // Unique semaphore ID (>= 1)
    std::string       name;            // Human-readable label (max 32 chars)
    SyncPrimitiveType primitiveType;   // SEMAPHORE_BINARY or SEMAPHORE_COUNTING
    int               value;           // Current count (0-1 for binary, 0-N for counting)
    int               initialValue;    // Initial count (1 for binary, >= 1 for counting)
    std::deque<int>   waitingPids;     // FIFO queue of blocked PIDs
    uint32_t          totalWaits;      // Cumulative wait() calls
    uint32_t          totalSignals;    // Cumulative signal() calls
    uint32_t          totalBlocks;     // Times wait() caused blocking (value was 0)

    // ── Default Constructor ──────────────────────────────────
    Semaphore()
        : semId(-1)
        , name("")
        , primitiveType(SyncPrimitiveType::SEMAPHORE_BINARY)
        , value(1)
        , initialValue(1)
        , waitingPids()
        , totalWaits(0)
        , totalSignals(0)
        , totalBlocks(0)
    {}
};
