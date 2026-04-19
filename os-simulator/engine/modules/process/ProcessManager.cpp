/**
 * ProcessManager.cpp — Process Manager Module Implementation
 *
 * Reference: SDD Section 3.1, PRD Section 6.1, DataDictionary Sections 3, 4, 8.2
 *
 * Phase 2 implementation of the Process Manager — the first OS subsystem module.
 *
 * onTick() sequence per clock tick:
 *   1. admitNewProcesses()    — NEW → READY, add to readyQueue
 *   2. handleIOCompletions()  — decrement I/O timers, WAITING → READY
 *   3. updateWaitingTimes()   — +1 waitingTime for each READY process
 */

#include "modules/process/ProcessManager.h"
#include "modules/process/ProcessSpec.h"
#include "modules/process/PCB.h"
#include "modules/process/TCB.h"
#include "core/SimulationState.h"
#include "core/EventBus.h"
#include "core/SimEvent.h"
#include "core/DecisionLogEntry.h"

#include <algorithm>
#include <random>

// ═══════════════════════════════════════════════════════════════
// Static helper — thread-local RNG for auto-assignment
// ═══════════════════════════════════════════════════════════════
namespace {
    std::mt19937& getRNG() {
        static std::mt19937 rng(std::random_device{}());
        return rng;
    }

    uint32_t randomInRange(uint32_t lo, uint32_t hi) {
        std::uniform_int_distribution<uint32_t> dist(lo, hi);
        return dist(getRNG());
    }
}

// ═══════════════════════════════════════════════════════════════
// Constructor / ISimModule interface
// ═══════════════════════════════════════════════════════════════

ProcessManager::ProcessManager()
    : status_(ModuleStatus::IDLE)
{}

std::string ProcessManager::getModuleName() const {
    return "ProcessManager";
}

ModuleStatus ProcessManager::getStatus() const {
    return status_;
}

void ProcessManager::reset() {
    status_ = ModuleStatus::IDLE;
}

// ═══════════════════════════════════════════════════════════════
// onTick() — Core per-tick logic
// ═══════════════════════════════════════════════════════════════

void ProcessManager::onTick(SimulationState& state, EventBus& bus) {
    // Step 1: Admit NEW processes to READY
    admitNewProcesses(state, bus);

    // Step 2: Handle I/O completions (WAITING → READY)
    handleIOCompletions(state, bus);

    // Step 3: Accumulate waiting time for READY processes
    updateWaitingTimes(state);

    // Mark module as active
    status_ = ModuleStatus::ACTIVE;
}

// ═══════════════════════════════════════════════════════════════
// admitNewProcesses() — NEW → READY
// ═══════════════════════════════════════════════════════════════

void ProcessManager::admitNewProcesses(SimulationState& state, EventBus& bus) {
    for (auto& [pid, pcb] : state.processTable) {
        if (pcb.state == ProcessState::NEW) {
            // Transition to READY
            pcb.state = ProcessState::READY;

            // Assign page table ID (keyed by PID)
            pcb.pageTableId = pid;

            // Add to back of ready queue
            state.readyQueue.push_back(pid);

            // Publish state change event
            bus.publish(SimEvent(
                state.currentTick,
                EventTypes::PROCESS_STATE_CHANGED,
                pid,     // source
                pid,     // target (self)
                -1,      // no resource
                "Process " + pcb.name + " (PID " + std::to_string(pid)
                    + ") admitted to ready queue"
            ));

            // Log decision
            state.decisionLog.emplace_back(
                state.currentTick,
                "Process " + pcb.name + " (PID " + std::to_string(pid)
                    + ") admitted: NEW → READY"
            );
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// handleIOCompletions() — WAITING → READY (I/O countdown)
// ═══════════════════════════════════════════════════════════════

void ProcessManager::handleIOCompletions(SimulationState& state, EventBus& bus) {
    for (auto& [pid, pcb] : state.processTable) {
        if (pcb.state == ProcessState::WAITING && pcb.ioRemainingTicks > 0) {
            // Decrement I/O timer
            pcb.ioRemainingTicks--;

            if (pcb.ioRemainingTicks == 0) {
                // I/O complete — move to READY
                pcb.state = ProcessState::READY;
                pcb.ioCompletionTick = 0;

                // Add to back of ready queue
                state.readyQueue.push_back(pid);

                // Publish state change event
                bus.publish(SimEvent(
                    state.currentTick,
                    EventTypes::PROCESS_STATE_CHANGED,
                    pid, pid, -1,
                    "Process " + pcb.name + " (PID " + std::to_string(pid)
                        + ") I/O completed, moved to READY"
                ));

                // Log decision
                state.decisionLog.emplace_back(
                    state.currentTick,
                    "Process " + pcb.name + " (PID " + std::to_string(pid)
                        + ") I/O complete: WAITING → READY"
                );
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// updateWaitingTimes() — +1 per tick for READY processes
// ═══════════════════════════════════════════════════════════════

void ProcessManager::updateWaitingTimes(SimulationState& state) {
    for (auto& [pid, pcb] : state.processTable) {
        if (pcb.state == ProcessState::READY) {
            pcb.waitingTime++;
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// createProcess() — Create a new process from a ProcessSpec
// ═══════════════════════════════════════════════════════════════

int ProcessManager::createProcess(SimulationState& state, EventBus& bus,
                                  const ProcessSpec& spec) {
    // Assign PID
    int pid = state.nextPID++;

    // Build PCB
    PCB pcb;
    pcb.pid = pid;
    pcb.name = spec.name.empty() ? autoGenerateName(pid) : spec.name;
    pcb.type = spec.type;
    pcb.priority = spec.priority;

    // State
    pcb.state = ProcessState::NEW;
    pcb.arrivalTick = state.currentTick;
    pcb.startTick = 0;
    pcb.terminationTick = 0;

    // CPU burst — auto-assign if 0
    pcb.totalCpuBurst = (spec.cpuBurst > 0) ? spec.cpuBurst : autoAssignBurst(spec.type);
    pcb.remainingBurst = pcb.totalCpuBurst;
    pcb.quantumUsed = 0;

    // I/O burst — auto-assign if 0
    pcb.ioBurstDuration = (spec.ioBurstDuration > 0)
        ? spec.ioBurstDuration
        : autoAssignIO(spec.type);
    pcb.ioRemainingTicks = 0;
    pcb.ioCompletionTick = 0;

    // Memory — auto-assign if 0
    pcb.memoryRequirement = (spec.memoryRequirement > 0)
        ? spec.memoryRequirement
        : autoAssignMemory(spec.type);
    pcb.pageTableId = -1;  // Assigned on admission (NEW → READY)

    // Metrics
    pcb.waitingTime = 0;
    pcb.turnaroundTime = 0;
    pcb.contextSwitches = 0;
    pcb.pageFaultCount = 0;

    // Threads
    pcb.threadIds.clear();

    // Insert into process table
    state.processTable[pid] = pcb;

    // Publish PROCESS_CREATED event
    bus.publish(SimEvent(
        state.currentTick,
        EventTypes::PROCESS_CREATED,
        pid, pid, -1,
        "Process " + pcb.name + " (PID " + std::to_string(pid)
            + ") created: burst=" + std::to_string(pcb.totalCpuBurst)
            + ", priority=" + std::to_string(pcb.priority)
            + ", type=" + toString(pcb.type)
    ));

    // Log decision
    state.decisionLog.emplace_back(
        state.currentTick,
        "Created process " + pcb.name + " (PID " + std::to_string(pid)
            + "): CPU burst=" + std::to_string(pcb.totalCpuBurst)
            + ", I/O burst=" + std::to_string(pcb.ioBurstDuration)
            + ", priority=" + std::to_string(pcb.priority)
            + ", memory=" + std::to_string(pcb.memoryRequirement) + " pages"
    );

    return pid;
}

// ═══════════════════════════════════════════════════════════════
// killProcess() — Force-terminate a process
// ═══════════════════════════════════════════════════════════════

void ProcessManager::killProcess(SimulationState& state, EventBus& bus, int pid) {
    // Find PCB
    auto it = state.processTable.find(pid);
    if (it == state.processTable.end()) {
        return;  // Non-existent PID — no-op
    }

    PCB& pcb = it->second;

    // Already terminated — no-op
    if (pcb.state == ProcessState::TERMINATED) {
        return;
    }

    // Record previous state for logging
    std::string prevState = toString(pcb.state);

    // Transition to TERMINATED
    pcb.state = ProcessState::TERMINATED;
    pcb.terminationTick = state.currentTick;
    pcb.turnaroundTime = pcb.terminationTick - pcb.arrivalTick;

    // Remove from ready queue (if present)
    auto rqIt = std::find(state.readyQueue.begin(), state.readyQueue.end(), pid);
    if (rqIt != state.readyQueue.end()) {
        state.readyQueue.erase(rqIt);
    }

    // If this was the running process, clear runningPID
    if (state.runningPID == pid) {
        state.runningPID = -1;
    }

    // Terminate all child threads
    for (int tid : pcb.threadIds) {
        auto tIt = state.threadTable.find(tid);
        if (tIt != state.threadTable.end()) {
            tIt->second.state = ThreadState::T_TERMINATED;
        }
    }

    // Publish PROCESS_TERMINATED event
    bus.publish(SimEvent(
        state.currentTick,
        EventTypes::PROCESS_TERMINATED,
        pid, pid, -1,
        "Process " + pcb.name + " (PID " + std::to_string(pid)
            + ") terminated (was " + prevState + ")"
            + ", turnaround=" + std::to_string(pcb.turnaroundTime)
    ));

    // Log decision
    state.decisionLog.emplace_back(
        state.currentTick,
        "Killed process " + pcb.name + " (PID " + std::to_string(pid)
            + "): " + prevState + " → TERMINATED"
            + ", turnaround=" + std::to_string(pcb.turnaroundTime) + " ticks"
    );
}

// ═══════════════════════════════════════════════════════════════
// createThread() — Create a thread within a process
// ═══════════════════════════════════════════════════════════════

int ProcessManager::createThread(SimulationState& state, EventBus& bus,
                                 int parentPid, uint32_t cpuBurst,
                                 uint32_t stackSize) {
    // Validate parent exists and is not terminated
    auto pIt = state.processTable.find(parentPid);
    if (pIt == state.processTable.end()) {
        return -1;
    }
    if (pIt->second.state == ProcessState::TERMINATED) {
        return -1;
    }

    // Assign TID
    int tid = state.nextTID++;

    // Build TCB
    TCB tcb;
    tcb.tid = tid;
    tcb.parentPid = parentPid;
    tcb.state = ThreadState::T_NEW;
    tcb.creationTick = state.currentTick;
    tcb.stackSize = stackSize;
    tcb.simulatedSP = 0;
    tcb.cpuBurst = cpuBurst;
    tcb.remainingBurst = cpuBurst;
    tcb.blockedOnSyncId = -1;
    tcb.waitingTime = 0;

    // Insert into thread table
    state.threadTable[tid] = tcb;

    // Link to parent process
    pIt->second.threadIds.push_back(tid);

    // Suppress unused parameter warning — bus reserved for future thread events
    (void)bus;

    return tid;
}

// ═══════════════════════════════════════════════════════════════
// Auto-assignment helpers
// ═══════════════════════════════════════════════════════════════

uint32_t ProcessManager::autoAssignBurst(ProcessType type) {
    switch (type) {
        case ProcessType::CPU_BOUND: return randomInRange(8, 20);
        case ProcessType::IO_BOUND:  return randomInRange(2, 6);
        case ProcessType::MIXED:     return randomInRange(4, 12);
    }
    return 5;  // fallback
}

uint32_t ProcessManager::autoAssignIO(ProcessType type) {
    switch (type) {
        case ProcessType::CPU_BOUND: return 0;  // No I/O for CPU-bound
        case ProcessType::IO_BOUND:  return randomInRange(3, 8);
        case ProcessType::MIXED:     return randomInRange(2, 5);
    }
    return 0;  // fallback
}

uint32_t ProcessManager::autoAssignMemory(ProcessType type) {
    // All types get 2–4 pages by default
    (void)type;
    return randomInRange(2, 4);
}

std::string ProcessManager::autoGenerateName(int pid) {
    return "proc_" + std::to_string(pid);
}
