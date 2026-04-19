#pragma once

/**
 * ISchedulingPolicy.h — Strategy Interface for CPU Scheduling Algorithms
 *
 * Reference: SDD Section 3.4 — Strategy Pattern for Policies
 *
 * Scheduling algorithms are implemented as strategy objects, not hardcoded logic.
 * The Scheduler module holds a pointer to the active ISchedulingPolicy.
 * Swapping a policy at runtime is a single pointer reassignment.
 *
 * Concrete implementations: FCFSPolicy, RoundRobinPolicy, PriorityPolicy
 */

#include <string>
#include <deque>
#include <map>

// Forward declaration
struct PCB;

class ISchedulingPolicy {
public:
    /**
     * Select the next process to run from the ready queue.
     *
     * @param readyQueue    The current ready queue (PIDs)
     * @param processTable  Read-only access to all PCBs for decision-making
     * @return PID of the selected process, or -1 if queue is empty
     */
    virtual int selectNext(std::deque<int>& readyQueue,
                           const std::map<int, PCB>& processTable) = 0;

    /**
     * Get the human-readable name of this policy.
     * Must match a SchedulingPolicy enum string value.
     */
    virtual std::string policyName() const = 0;

    /**
     * Virtual destructor for proper cleanup.
     */
    virtual ~ISchedulingPolicy() = default;
};
