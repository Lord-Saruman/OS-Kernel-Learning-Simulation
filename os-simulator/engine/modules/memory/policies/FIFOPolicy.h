#pragma once

/**
 * FIFOPolicy.h — First-In-First-Out Page Replacement Policy
 *
 * Reference: PRD FR-MM-03, DataDictionary §2.6 (FIFO)
 *
 * Evicts the page that was loaded into memory earliest — the frame
 * with the smallest loadTick value. This is the simplest replacement
 * algorithm and serves as a baseline for comparison against LRU.
 *
 * This is a header-only implementation (strategy is lightweight).
 */

#include "modules/memory/IReplacementPolicy.h"
#include "modules/memory/FrameTableEntry.h"

#include <vector>
#include <cstdint>
#include <climits>

class FIFOPolicy : public IReplacementPolicy {
public:
    /**
     * Select the victim frame for eviction using FIFO order.
     * Scans all occupied frames and returns the one with the
     * earliest (smallest) loadTick — the oldest loaded page.
     *
     * @param frames  The current frame table (all physical frames)
     * @return Index (frameNumber) of the frame to evict, or -1 if none occupied
     */
    int selectVictimFrame(const std::vector<FrameTableEntry>& frames) override {
        int victimIdx = -1;
        uint64_t oldestTick = UINT64_MAX;

        for (const auto& frame : frames) {
            if (frame.occupied && frame.loadTick < oldestTick) {
                oldestTick = frame.loadTick;
                victimIdx = frame.frameNumber;
            }
        }

        return victimIdx;
    }

    std::string policyName() const override {
        return "FIFO";
    }
};
