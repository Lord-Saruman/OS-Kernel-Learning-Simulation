#pragma once

/**
 * PageTableEntry.h — Page Table Entry (PTE)
 *
 * Reference: DataDictionary_MiniOS_Simulator.md, Section 7.1
 * One per virtual page per process
 *
 * Maps one virtual page number to a physical frame number. Tracks
 * validity and reference metadata used by page replacement algorithms.
 */

#include <cstdint>

struct PageTableEntry {
    uint32_t  virtualPageNumber;  // VPN — index into process's page table
    int       frameNumber;        // Physical frame (-1 if not in memory)
    bool      valid;              // true if loaded in a frame (false → page fault)
    bool      dirty;              // true if written since load (for future write-back)
    bool      referenced;         // true if accessed this tick (cleared each tick)
    uint64_t  loadTick;           // Tick when page was loaded (FIFO uses this)
    uint64_t  lastAccessTick;     // Tick of most recent access (LRU uses this)

    // ── Default Constructor ──────────────────────────────────
    PageTableEntry()
        : virtualPageNumber(0)
        , frameNumber(-1)
        , valid(false)
        , dirty(false)
        , referenced(false)
        , loadTick(0)
        , lastAccessTick(0)
    {}
};
