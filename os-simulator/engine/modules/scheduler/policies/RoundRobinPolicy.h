#pragma once

/**
 * RoundRobinPolicy.h — Round Robin Scheduling Policy
 *
 * Reference: PRD FR-SC-02, DataDictionary §2.5 (ROUND_ROBIN)
 *
 * Preemptive policy. Selects the next process from the front of the
 * ready queue (FIFO). Quantum enforcement is handled by the Scheduler
 * module (Scheduler::handleQuantumExpiry), not by this policy.
 *
 * When a process's quantum expires, the Scheduler pushes it to the
 * back of the ready queue, so round-robin rotation is maintained.
 *
 * This is a header-only implementation (strategy is lightweight).
 */

#include "modules/scheduler/ISchedulingPolicy.h"
#include "modules/process/PCB.h"

#include <deque>
#include <map>

class RoundRobinPolicy : public ISchedulingPolicy {
public:
    /**
     * Select the next process from the front of the ready queue.
     * RR rotation is maintained by the Scheduler pushing quantum-expired
     * processes to the back of the queue.
     *
     * @param readyQueue    Ready queue of PIDs (modified — selected PID removed)
     * @param processTable  Read-only PCB map (unused for RR, but part of interface)
     * @return PID of selected process, or -1 if queue empty
     */
    int selectNext(std::deque<int>& readyQueue,
                   const std::map<int, PCB>& processTable) override {
        (void)processTable;  // RR doesn't inspect PCB fields for selection

        if (readyQueue.empty()) {
            return -1;
        }

        int selectedPid = readyQueue.front();
        readyQueue.pop_front();
        return selectedPid;
    }

    std::string policyName() const override {
        return "ROUND_ROBIN";
    }
};
