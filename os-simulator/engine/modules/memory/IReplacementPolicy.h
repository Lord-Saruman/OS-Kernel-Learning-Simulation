#pragma once

/**
 * IReplacementPolicy.h — Strategy Interface for Page Replacement Algorithms
 *
 * Reference: SDD Section 3.4 — Strategy Pattern for Policies
 *
 * Page replacement policies are implemented as strategy objects.
 * The Memory Manager holds a pointer to the active IReplacementPolicy.
 * Swapping a policy at runtime is a single pointer reassignment.
 *
 * Concrete implementations: FIFOPolicy, LRUPolicy
 */

#include <string>
#include <vector>

// Forward declaration
struct FrameTableEntry;

class IReplacementPolicy {
public:
    /**
     * Select a victim frame for page replacement.
     *
     * @param frames  The current frame table (all physical frames)
     * @return Index (frameNumber) of the frame to evict
     */
    virtual int selectVictimFrame(const std::vector<FrameTableEntry>& frames) = 0;

    /**
     * Get the human-readable name of this policy.
     * Must match a ReplacementPolicy enum string value.
     */
    virtual std::string policyName() const = 0;

    /**
     * Virtual destructor for proper cleanup.
     */
    virtual ~IReplacementPolicy() = default;
};
