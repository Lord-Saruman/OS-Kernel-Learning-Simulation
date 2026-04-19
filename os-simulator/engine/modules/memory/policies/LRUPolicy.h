#pragma once

/**
 * LRUPolicy.h — Least Recently Used Page Replacement Policy
 *
 * Reference: PRD FR-MM-04, DataDictionary §2.6 (LRU)
 *
 * Evicts the page that was accessed least recently — the frame
 * with the smallest lastAccessTick value. LRU is generally optimal
 * among practical algorithms and produces fewer page faults than FIFO
 * for most reference patterns.
 *
 * This is a header-only implementation (strategy is lightweight).
 */

#include "modules/memory/IReplacementPolicy.h"
#include "modules/memory/FrameTableEntry.h"

#include <vector>
#include <cstdint>
#include <climits>

class LRUPolicy : public IReplacementPolicy {
public:
    /**
     * Select the victim frame for eviction using LRU order.
     * Scans all occupied frames and returns the one with the
     * earliest (smallest) lastAccessTick — the least recently used page.
     *
     * @param frames  The current frame table (all physical frames)
     * @return Index (frameNumber) of the frame to evict, or -1 if none occupied
     */
    int selectVictimFrame(const std::vector<FrameTableEntry>& frames) override {
        int victimIdx = -1;
        uint64_t oldestAccess = UINT64_MAX;

        for (const auto& frame : frames) {
            if (frame.occupied && frame.lastAccessTick < oldestAccess) {
                oldestAccess = frame.lastAccessTick;
                victimIdx = frame.frameNumber;
            }
        }

        return victimIdx;
    }

    std::string policyName() const override {
        return "LRU";
    }
};
