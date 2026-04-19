#pragma once

/**
 * FCFSPolicy.h — First-Come-First-Served Scheduling Policy
 *
 * Reference: PRD FR-SC-01, DataDictionary §2.5 (FCFS)
 *
 * Non-preemptive policy. Selects the process that arrived first
 * in the ready queue (FIFO order). Once dispatched, a process runs
 * until it completes its burst or enters an I/O wait.
 *
 * This is a header-only implementation (strategy is lightweight).
 */

#include "modules/scheduler/ISchedulingPolicy.h"
#include "modules/process/PCB.h"

#include <deque>
#include <map>
#include <algorithm>

class FCFSPolicy : public ISchedulingPolicy {
public:
    /**
     * Select the process with the earliest arrival time from the ready queue.
     * In practice, the ready queue is already ordered by arrival (new processes
     * are pushed to the back), so this simply pops the front.
     *
     * @param readyQueue    Ready queue of PIDs (modified — selected PID removed)
     * @param processTable  Read-only PCB map for accessing arrivalTick
     * @return PID of selected process, or -1 if queue empty
     */
    int selectNext(std::deque<int>& readyQueue,
                   const std::map<int, PCB>& processTable) override {
        if (readyQueue.empty()) {
            return -1;
        }

        // Sort by arrivalTick to guarantee FCFS order
        // (queue may have been disturbed by preemption from a previous policy)
        std::sort(readyQueue.begin(), readyQueue.end(),
            [&processTable](int a, int b) {
                auto itA = processTable.find(a);
                auto itB = processTable.find(b);
                if (itA == processTable.end()) return false;
                if (itB == processTable.end()) return true;
                return itA->second.arrivalTick < itB->second.arrivalTick;
            });

        int selectedPid = readyQueue.front();
        readyQueue.pop_front();
        return selectedPid;
    }

    std::string policyName() const override {
        return "FCFS";
    }
};
