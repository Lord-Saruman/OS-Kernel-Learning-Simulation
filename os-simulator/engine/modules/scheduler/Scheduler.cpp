/**
 * Scheduler.cpp — CPU Scheduler Module Implementation
 *
 * Reference: SDD Section 3.1, 3.2, 3.4; PRD Section 6.2;
 *            DataDictionary Sections 2.5, 5.1, 5.2
 *
 * Phase 3 implementation of the CPU Scheduler — the second OS subsystem module.
 *
 * onTick() sequence per clock tick:
 *   1. handleIOInitiation()   — CPU segment complete? → RUNNING→WAITING
 *   2. handleQuantumExpiry()  — RR quantum expired?   → RUNNING→READY (back of queue)
 *   3. checkPreemption()      — PRIORITY_P: higher-priority arrival? → preempt
 *   4. dispatchProcess()      — CPU idle? → select via policy, READY→RUNNING
 *   5. decrementBurst()       — running? → remainingBurst--, burst==0 → TERMINATED
 *   6. logGanttEntry()        — record this tick in Gantt log
 *   7. updateMetrics()        — recompute SchedulingMetrics
 */

#include "modules/scheduler/Scheduler.h"
#include "modules/scheduler/ISchedulingPolicy.h"
#include "modules/scheduler/GanttEntry.h"
#include "modules/scheduler/SchedulingMetrics.h"
#include "modules/scheduler/policies/FCFSPolicy.h"
#include "modules/scheduler/policies/RoundRobinPolicy.h"
#include "modules/scheduler/policies/PriorityPolicy.h"
#include "modules/process/PCB.h"
#include "core/SimulationState.h"
#include "core/EventBus.h"
#include "core/SimEvent.h"
#include "core/DecisionLogEntry.h"

#include <algorithm>
#include <string>
#include <stdexcept>

// ═══════════════════════════════════════════════════════════════
// Constructor / ISimModule interface
// ═══════════════════════════════════════════════════════════════

Scheduler::Scheduler()
    : status_(ModuleStatus::IDLE)
    , activePolicy_(std::make_unique<FCFSPolicy>())
    , previousRunningPID_(-1)
    , busyTicks_(0)
{}

std::string Scheduler::getModuleName() const {
    return "Scheduler";
}

ModuleStatus Scheduler::getStatus() const {
    return status_;
}

void Scheduler::reset() {
    status_ = ModuleStatus::IDLE;
    activePolicy_ = std::make_unique<FCFSPolicy>();
    previousRunningPID_ = -1;
    busyTicks_ = 0;
}

// ═══════════════════════════════════════════════════════════════
// onTick() — Core per-tick logic
// ═══════════════════════════════════════════════════════════════

void Scheduler::onTick(SimulationState& state, EventBus& bus) {
    // ── Phase A: Handle deferred transitions from PREVIOUS tick ──
    // These check quantumUsed which was incremented by the last tick's
    // decrementBurst(), so they correctly reflect the previous tick's work.

    // Step 1: If running process completed its CPU segment → I/O
    handleIOInitiation(state, bus);

    // Step 2: If RR and quantum expired, preempt running process
    handleQuantumExpiry(state, bus);

    // Step 3: If PRIORITY_P, check for higher-priority arrivals
    checkPreemption(state, bus);

    // ── Phase B: Dispatch for THIS tick ──

    // Step 4: If CPU idle, dispatch next process from ready queue
    dispatchProcess(state, bus);

    // ── Phase C: Record and execute THIS tick ──

    // Step 5: Log Gantt entry BEFORE decrement (record who runs this tick)
    logGanttEntry(state);

    // Step 6: Execute this tick's work (decrement burst, check completion)
    decrementBurst(state, bus);

    // Step 7: Recompute metrics
    updateMetrics(state);

    // Mark module as active
    status_ = ModuleStatus::ACTIVE;
}

// ═══════════════════════════════════════════════════════════════
// handleIOInitiation() — RUNNING → WAITING (CPU→I/O burst cycle)
// ═══════════════════════════════════════════════════════════════

void Scheduler::handleIOInitiation(SimulationState& state, EventBus& bus) {
    if (state.runningPID == -1) return;

    auto it = state.processTable.find(state.runningPID);
    if (it == state.processTable.end()) return;

    PCB& pcb = it->second;

    // Check deterministic burst cycle:
    // Process has I/O, has remaining work, and has completed a CPU segment
    if (pcb.ioBurstDuration > 0
        && pcb.remainingBurst > 0
        && pcb.quantumUsed >= pcb.cpuSegmentLength) {

        // Transition to WAITING for I/O
        pcb.state = ProcessState::WAITING;
        pcb.ioRemainingTicks = pcb.ioBurstDuration;
        pcb.ioCompletionTick = state.currentTick + pcb.ioBurstDuration;
        pcb.quantumUsed = 0;  // Reset for next CPU segment

        int pid = state.runningPID;
        state.runningPID = -1;

        // Publish state change event
        bus.publish(SimEvent(
            state.currentTick,
            EventTypes::PROCESS_STATE_CHANGED,
            pid, pid, -1,
            "Process " + pcb.name + " (PID " + std::to_string(pid)
                + ") entering I/O burst (" + std::to_string(pcb.ioBurstDuration)
                + " ticks) — CPU segment of " + std::to_string(pcb.cpuSegmentLength)
                + " ticks completed"
        ));

        // Log decision
        state.decisionLog.emplace_back(
            state.currentTick,
            "Process " + pcb.name + " (PID " + std::to_string(pid)
                + "): RUNNING → WAITING (I/O burst, "
                + std::to_string(pcb.remainingBurst) + " CPU ticks remaining)"
        );
    }
}

// ═══════════════════════════════════════════════════════════════
// handleQuantumExpiry() — RR: RUNNING → READY (quantum expired)
// ═══════════════════════════════════════════════════════════════

void Scheduler::handleQuantumExpiry(SimulationState& state, EventBus& bus) {
    // Only applies to Round Robin
    if (activePolicy_->policyName() != "ROUND_ROBIN") return;
    if (state.runningPID == -1) return;

    auto it = state.processTable.find(state.runningPID);
    if (it == state.processTable.end()) return;

    PCB& pcb = it->second;

    // Check if quantum expired
    if (pcb.quantumUsed >= state.timeQuantum) {
        int pid = state.runningPID;

        // Preempt: RUNNING → READY
        pcb.state = ProcessState::READY;
        pcb.quantumUsed = 0;
        pcb.contextSwitches++;

        // Push to back of ready queue (RR rotation)
        state.readyQueue.push_back(pid);
        state.runningPID = -1;

        // Publish context switch event
        bus.publish(SimEvent(
            state.currentTick,
            EventTypes::CONTEXT_SWITCH,
            pid, pid, -1,
            "Process " + pcb.name + " (PID " + std::to_string(pid)
                + ") preempted — quantum expired ("
                + std::to_string(state.timeQuantum) + " ticks)"
        ));

        // Log decision
        state.decisionLog.emplace_back(
            state.currentTick,
            "RR quantum expired for " + pcb.name + " (PID "
                + std::to_string(pid) + "): RUNNING → READY (back of queue)"
        );
    }
}

// ═══════════════════════════════════════════════════════════════
// checkPreemption() — PRIORITY_P: preempt if higher-priority arrived
// ═══════════════════════════════════════════════════════════════

void Scheduler::checkPreemption(SimulationState& state, EventBus& bus) {
    // Only applies to preemptive priority
    PriorityPolicy* pp = dynamic_cast<PriorityPolicy*>(activePolicy_.get());
    if (!pp || !pp->isPreemptive()) return;
    if (state.runningPID == -1) return;
    if (state.readyQueue.empty()) return;

    auto runIt = state.processTable.find(state.runningPID);
    if (runIt == state.processTable.end()) return;

    int runningPriority = runIt->second.priority;

    // Find the highest-priority process in the ready queue
    int bestPid = -1;
    int bestPriority = runningPriority;  // Must be strictly higher (lower value)

    for (int pid : state.readyQueue) {
        auto pcbIt = state.processTable.find(pid);
        if (pcbIt == state.processTable.end()) continue;

        if (pcbIt->second.priority < bestPriority) {
            bestPriority = pcbIt->second.priority;
            bestPid = pid;
        }
    }

    if (bestPid == -1) return;  // No higher-priority process found

    // Preempt running process
    int preemptedPid = state.runningPID;
    PCB& preemptedPcb = runIt->second;

    preemptedPcb.state = ProcessState::READY;
    preemptedPcb.contextSwitches++;
    // Note: do NOT reset quantumUsed — it tracks CPU segment progress for I/O

    // Insert preempted process at front of ready queue (it was running)
    state.readyQueue.push_front(preemptedPid);
    state.runningPID = -1;

    // Publish context switch event
    bus.publish(SimEvent(
        state.currentTick,
        EventTypes::CONTEXT_SWITCH,
        bestPid, preemptedPid, -1,
        "Process PID " + std::to_string(bestPid)
            + " (priority " + std::to_string(bestPriority)
            + ") preempts PID " + std::to_string(preemptedPid)
            + " (priority " + std::to_string(runningPriority) + ")"
    ));

    // Log decision
    state.decisionLog.emplace_back(
        state.currentTick,
        "Priority preemption: PID " + std::to_string(bestPid)
            + " (pri=" + std::to_string(bestPriority)
            + ") preempts PID " + std::to_string(preemptedPid)
            + " (pri=" + std::to_string(runningPriority) + ")"
    );
}

// ═══════════════════════════════════════════════════════════════
// dispatchProcess() — READY → RUNNING (policy selects next)
// ═══════════════════════════════════════════════════════════════

void Scheduler::dispatchProcess(SimulationState& state, EventBus& bus) {
    // Only dispatch if CPU is idle
    if (state.runningPID != -1) return;
    if (state.readyQueue.empty()) return;

    // Use active policy to select next process
    int selectedPid = activePolicy_->selectNext(state.readyQueue, state.processTable);
    if (selectedPid == -1) return;

    auto it = state.processTable.find(selectedPid);
    if (it == state.processTable.end()) return;

    PCB& pcb = it->second;

    // Dispatch: READY → RUNNING
    pcb.state = ProcessState::RUNNING;
    state.runningPID = selectedPid;

    // First dispatch: record startTick
    if (pcb.startTick == 0) {
        pcb.startTick = state.currentTick;
    }

    // Reset quantum counter for this dispatch
    pcb.quantumUsed = 0;

    // Update active policy name in state (for Gantt snapshots)
    state.activePolicy = activePolicy_->policyName();

    // Detect context switch
    if (selectedPid != previousRunningPID_) {
        if (previousRunningPID_ != -1) {
            state.metrics.totalContextSwitches++;
        }

        // Publish context switch event (only if there was a previous process)
        if (previousRunningPID_ != -1) {
            bus.publish(SimEvent(
                state.currentTick,
                EventTypes::CONTEXT_SWITCH,
                previousRunningPID_, selectedPid, -1,
                "Context switch: PID " + std::to_string(previousRunningPID_)
                    + " → PID " + std::to_string(selectedPid)
            ));
        }

        previousRunningPID_ = selectedPid;
    }

    // Build reason string based on policy
    std::string reason;
    std::string policyName = activePolicy_->policyName();
    if (policyName == "FCFS") {
        reason = "first in queue (arrival order)";
    } else if (policyName == "ROUND_ROBIN") {
        reason = "next in rotation (quantum=" + std::to_string(state.timeQuantum) + ")";
    } else if (policyName == "PRIORITY_NP" || policyName == "PRIORITY_P") {
        reason = "highest priority (pri=" + std::to_string(pcb.priority) + ")";
    } else {
        reason = "policy selection";
    }

    // Publish CPU_SCHEDULED event
    bus.publish(SimEvent(
        state.currentTick,
        EventTypes::CPU_SCHEDULED,
        selectedPid, selectedPid, -1,
        "Scheduled " + pcb.name + " (PID " + std::to_string(selectedPid)
            + ") via " + policyName + " — " + reason
    ));

    // Log decision
    state.decisionLog.emplace_back(
        state.currentTick,
        "Dispatched " + pcb.name + " (PID " + std::to_string(selectedPid)
            + "): READY → RUNNING [" + policyName + ": " + reason + "]"
    );
}

// ═══════════════════════════════════════════════════════════════
// decrementBurst() — running process: burst--, check completion
// ═══════════════════════════════════════════════════════════════

void Scheduler::decrementBurst(SimulationState& state, EventBus& bus) {
    if (state.runningPID == -1) return;

    auto it = state.processTable.find(state.runningPID);
    if (it == state.processTable.end()) return;

    PCB& pcb = it->second;

    // Decrement burst
    if (pcb.remainingBurst > 0) {
        pcb.remainingBurst--;
    }
    pcb.quantumUsed++;

    // Check for completion
    if (pcb.remainingBurst == 0) {
        int pid = state.runningPID;

        // Terminate
        pcb.state = ProcessState::TERMINATED;
        pcb.terminationTick = state.currentTick;
        pcb.turnaroundTime = pcb.terminationTick - pcb.arrivalTick;

        state.runningPID = -1;
        previousRunningPID_ = -1;

        // Publish termination event
        bus.publish(SimEvent(
            state.currentTick,
            EventTypes::PROCESS_TERMINATED,
            pid, pid, -1,
            "Process " + pcb.name + " (PID " + std::to_string(pid)
                + ") completed — turnaround=" + std::to_string(pcb.turnaroundTime)
                + ", waiting=" + std::to_string(pcb.waitingTime)
        ));

        // Log decision
        state.decisionLog.emplace_back(
            state.currentTick,
            "Process " + pcb.name + " (PID " + std::to_string(pid)
                + ") burst complete: RUNNING → TERMINATED"
                + " (turnaround=" + std::to_string(pcb.turnaroundTime)
                + ", wait=" + std::to_string(pcb.waitingTime) + ")"
        );
    }
}

// ═══════════════════════════════════════════════════════════════
// logGanttEntry() — Record this tick in the Gantt chart log
// ═══════════════════════════════════════════════════════════════

void Scheduler::logGanttEntry(SimulationState& state) {
    state.ganttLog.emplace_back(
        state.currentTick,
        state.runningPID,
        activePolicy_->policyName()
    );

    // Track busy ticks for CPU utilization
    if (state.runningPID != -1) {
        busyTicks_++;
    }
}

// ═══════════════════════════════════════════════════════════════
// updateMetrics() — Recompute SchedulingMetrics
// ═══════════════════════════════════════════════════════════════

void Scheduler::updateMetrics(SimulationState& state) {
    uint32_t completed = 0;
    uint32_t total = 0;
    uint64_t totalWaiting = 0;
    uint64_t totalTurnaround = 0;

    for (const auto& [pid, pcb] : state.processTable) {
        total++;
        if (pcb.state == ProcessState::TERMINATED) {
            completed++;
            totalWaiting += pcb.waitingTime;
            totalTurnaround += pcb.turnaroundTime;
        }
    }

    state.metrics.completedProcesses = completed;
    state.metrics.totalProcesses = total;

    if (completed > 0) {
        state.metrics.avgWaitingTime =
            static_cast<float>(totalWaiting) / static_cast<float>(completed);
        state.metrics.avgTurnaroundTime =
            static_cast<float>(totalTurnaround) / static_cast<float>(completed);
    } else {
        state.metrics.avgWaitingTime = 0.0f;
        state.metrics.avgTurnaroundTime = 0.0f;
    }

    // CPU utilization: % of ticks CPU was occupied
    if (state.currentTick > 0) {
        state.metrics.cpuUtilization =
            (static_cast<float>(busyTicks_) / static_cast<float>(state.currentTick)) * 100.0f;
    } else {
        state.metrics.cpuUtilization = 0.0f;
    }

    // Throughput: processes completed per 100 ticks
    if (state.currentTick > 0) {
        state.metrics.throughput =
            static_cast<uint32_t>((static_cast<float>(completed) / static_cast<float>(state.currentTick)) * 100.0f);
    } else {
        state.metrics.throughput = 0;
    }
}

// ═══════════════════════════════════════════════════════════════
// Policy Management
// ═══════════════════════════════════════════════════════════════

void Scheduler::setPolicy(const std::string& policyName) {
    auto newPolicy = createPolicy(policyName);
    if (newPolicy) {
        activePolicy_ = std::move(newPolicy);
    }
}

std::string Scheduler::getActivePolicyName() const {
    return activePolicy_ ? activePolicy_->policyName() : "NONE";
}

std::unique_ptr<ISchedulingPolicy> Scheduler::createPolicy(const std::string& name) {
    if (name == "FCFS") {
        return std::make_unique<FCFSPolicy>();
    } else if (name == "ROUND_ROBIN") {
        return std::make_unique<RoundRobinPolicy>();
    } else if (name == "PRIORITY_NP") {
        return std::make_unique<PriorityPolicy>(false);
    } else if (name == "PRIORITY_P") {
        return std::make_unique<PriorityPolicy>(true);
    }
    // Unknown policy — return nullptr (caller should handle)
    return nullptr;
}
