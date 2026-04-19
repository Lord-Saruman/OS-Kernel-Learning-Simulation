#pragma once

/**
 * PriorityPolicy.h — Priority Scheduling Policy (Preemptive & Non-Preemptive)
 *
 * Reference: PRD FR-SC-03, DataDictionary §2.5 (PRIORITY_NP, PRIORITY_P)
 *
 * Selects the process with the lowest priority value (= highest priority)
 * from the ready queue. Tie-breaking: FCFS order (earlier arrivalTick wins).
 *
 * A single class handles both preemptive (PRIORITY_P) and non-preemptive
 * (PRIORITY_NP) modes. The preemptive flag is used by the Scheduler's
 * checkPreemption() method — this policy's selectNext() is identical
 * for both modes.
 *
 * This is a header-only implementation (strategy is lightweight).
 */

#include "modules/scheduler/ISchedulingPolicy.h"
#include "modules/process/PCB.h"

#include <deque>
#include <map>
#include <algorithm>

class PriorityPolicy : public ISchedulingPolicy {
public:
    /**
     * @param preemptive  true = PRIORITY_P, false = PRIORITY_NP
     */
    explicit PriorityPolicy(bool preemptive)
        : preemptive_(preemptive)
    {}

    /**
     * Select the process with the lowest priority value (highest priority).
     * Tie-breaking: process with earlier arrivalTick wins.
     *
     * @param readyQueue    Ready queue of PIDs (modified — selected PID removed)
     * @param processTable  Read-only PCB map for accessing priority and arrivalTick
     * @return PID of selected process, or -1 if queue empty
     */
    int selectNext(std::deque<int>& readyQueue,
                   const std::map<int, PCB>& processTable) override {
        if (readyQueue.empty()) {
            return -1;
        }

        // Find the highest-priority process (lowest priority value)
        // Tie-break by arrivalTick (FCFS among equal priorities)
        auto bestIt = readyQueue.begin();
        for (auto it = readyQueue.begin(); it != readyQueue.end(); ++it) {
            auto currPcb = processTable.find(*it);
            auto bestPcb = processTable.find(*bestIt);

            if (currPcb == processTable.end()) continue;
            if (bestPcb == processTable.end()) {
                bestIt = it;
                continue;
            }

            // Lower priority value = higher priority
            if (currPcb->second.priority < bestPcb->second.priority) {
                bestIt = it;
            } else if (currPcb->second.priority == bestPcb->second.priority) {
                // Tie-break: earlier arrival wins
                if (currPcb->second.arrivalTick < bestPcb->second.arrivalTick) {
                    bestIt = it;
                }
            }
        }

        int selectedPid = *bestIt;
        readyQueue.erase(bestIt);
        return selectedPid;
    }

    std::string policyName() const override {
        return preemptive_ ? "PRIORITY_P" : "PRIORITY_NP";
    }

    /**
     * Whether this policy is preemptive.
     * Used by Scheduler::checkPreemption() to decide whether to preempt
     * the running process when a higher-priority process arrives.
     */
    bool isPreemptive() const { return preemptive_; }

private:
    bool preemptive_;
};
