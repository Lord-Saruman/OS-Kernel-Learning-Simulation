#pragma once

/**
 * MemoryMetrics.h — Memory Management Performance Metrics
 *
 * Reference: DataDictionary_MiniOS_Simulator.md, Section 7.4
 * Stored in: SimulationState::memMetrics
 */

#include <string>
#include <cstdint>

struct MemoryMetrics {
    uint32_t    totalFrames;        // Total physical frames (>= 4, immutable during sim)
    uint32_t    occupiedFrames;     // Currently occupied frames
    uint32_t    totalPageFaults;    // Cumulative page faults across all processes
    float       pageFaultRate;      // Faults per 100 memory accesses (0.0–100.0)
    uint32_t    totalReplacements;  // Page evictions triggered by replacement algorithm
    std::string activePolicy;       // Current replacement policy name ("FIFO" | "LRU")

    // ── Default Constructor ──────────────────────────────────
    MemoryMetrics()
        : totalFrames(16)
        , occupiedFrames(0)
        , totalPageFaults(0)
        , pageFaultRate(0.0f)
        , totalReplacements(0)
        , activePolicy("FIFO")
    {}
};
