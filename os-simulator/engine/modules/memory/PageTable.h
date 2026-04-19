#pragma once

/**
 * PageTable.h — Per-Process Page Table
 *
 * Reference: DataDictionary_MiniOS_Simulator.md, Section 7.2
 * One per process
 * Stored in: SimulationState::pageTables
 */

#include <vector>
#include <cstdint>

#include "modules/memory/PageTableEntry.h"

struct PageTable {
    int                        ownerPid;   // PID of owning process
    uint32_t                   pageSize;   // Page size in simulated bytes (power of 2, 64-1024, default 256)
    std::vector<PageTableEntry> entries;   // Indexed by VPN; length == PCB.memoryRequirement

    // ── Default Constructor ──────────────────────────────────
    PageTable()
        : ownerPid(-1)
        , pageSize(256)
        , entries()
    {}
};
