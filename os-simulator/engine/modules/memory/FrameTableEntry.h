#pragma once

/**
 * FrameTableEntry.h — Physical Memory Frame Entry
 *
 * Reference: DataDictionary_MiniOS_Simulator.md, Section 7.3
 * One per physical frame
 * Stored in: SimulationState::frameTable (as std::vector<FrameTableEntry>)
 *
 * Represents one physical memory frame in simulated RAM.
 * Tracks which page from which process currently occupies each frame.
 */

#include <cstdint>

struct FrameTableEntry {
    int       frameNumber;        // Physical frame index (0 to totalFrames-1)
    bool      occupied;           // true if a page is loaded here
    int       ownerPid;           // PID of process whose page is here (-1 if free)
    uint32_t  virtualPageNumber;  // VPN of the loaded page (valid only when occupied)
    uint64_t  loadTick;           // Tick when current page was loaded (FIFO uses this)
    uint64_t  lastAccessTick;     // Most recent access tick (LRU uses this)

    // ── Default Constructor ──────────────────────────────────
    FrameTableEntry()
        : frameNumber(0)
        , occupied(false)
        , ownerPid(-1)
        , virtualPageNumber(0)
        , loadTick(0)
        , lastAccessTick(0)
    {}
};
