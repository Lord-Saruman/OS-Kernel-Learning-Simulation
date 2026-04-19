/**
 * test_simulation_state.cpp — Unit Tests for SimulationState
 *
 * Verifies that SimulationState initialises with correct defaults
 * matching the SDD and Data Dictionary specifications, and that
 * reset() properly clears all state.
 */

#include <gtest/gtest.h>

#include "core/SimulationState.h"
#include "core/SimEnums.h"

// ══════════════════════════════════════════════════════════════
// Default Initialisation Tests
// ══════════════════════════════════════════════════════════════

TEST(SimulationStateTest, DefaultClockValues) {
    SimulationState state;

    EXPECT_EQ(state.currentTick, 0u);
    EXPECT_EQ(state.mode, SimMode::STEP);
    EXPECT_EQ(state.status, SimStatus::IDLE);
    EXPECT_EQ(state.autoSpeedMs, 500u);
}

TEST(SimulationStateTest, DefaultProcessManagerValues) {
    SimulationState state;

    EXPECT_TRUE(state.processTable.empty());
    EXPECT_TRUE(state.threadTable.empty());
    EXPECT_EQ(state.nextPID, 1);
    EXPECT_EQ(state.nextTID, 1);
}

TEST(SimulationStateTest, DefaultSchedulerValues) {
    SimulationState state;

    EXPECT_TRUE(state.readyQueue.empty());
    EXPECT_EQ(state.runningPID, -1);
    EXPECT_EQ(state.activePolicy, "FCFS");
    EXPECT_EQ(state.timeQuantum, 2u);
    EXPECT_TRUE(state.ganttLog.empty());
    EXPECT_FLOAT_EQ(state.metrics.avgWaitingTime, 0.0f);
    EXPECT_FLOAT_EQ(state.metrics.avgTurnaroundTime, 0.0f);
    EXPECT_FLOAT_EQ(state.metrics.cpuUtilization, 0.0f);
    EXPECT_EQ(state.metrics.totalContextSwitches, 0u);
    EXPECT_EQ(state.metrics.completedProcesses, 0u);
    EXPECT_EQ(state.metrics.totalProcesses, 0u);
}

TEST(SimulationStateTest, DefaultSyncManagerValues) {
    SimulationState state;

    EXPECT_TRUE(state.mutexTable.empty());
    EXPECT_TRUE(state.semaphoreTable.empty());
    EXPECT_TRUE(state.blockedQueues.empty());
}

TEST(SimulationStateTest, DefaultMemoryManagerValues) {
    SimulationState state;

    EXPECT_TRUE(state.frameTable.empty());
    EXPECT_TRUE(state.pageTables.empty());
    EXPECT_EQ(state.activeReplacement, "FIFO");
    EXPECT_EQ(state.memMetrics.totalFrames, 16u);
    EXPECT_EQ(state.memMetrics.occupiedFrames, 0u);
    EXPECT_EQ(state.memMetrics.totalPageFaults, 0u);
    EXPECT_FLOAT_EQ(state.memMetrics.pageFaultRate, 0.0f);
    EXPECT_EQ(state.memMetrics.totalReplacements, 0u);
    EXPECT_EQ(state.memMetrics.activePolicy, "FIFO");
}

TEST(SimulationStateTest, DefaultDecisionLog) {
    SimulationState state;
    EXPECT_TRUE(state.decisionLog.empty());
}

// ══════════════════════════════════════════════════════════════
// PCB Tests
// ══════════════════════════════════════════════════════════════

TEST(PCBTest, DefaultValues) {
    PCB pcb;

    EXPECT_EQ(pcb.pid, -1);
    EXPECT_EQ(pcb.name, "");
    EXPECT_EQ(pcb.type, ProcessType::CPU_BOUND);
    EXPECT_EQ(pcb.priority, 5);
    EXPECT_EQ(pcb.state, ProcessState::NEW);
    EXPECT_EQ(pcb.arrivalTick, 0u);
    EXPECT_EQ(pcb.startTick, 0u);
    EXPECT_EQ(pcb.terminationTick, 0u);
    EXPECT_EQ(pcb.totalCpuBurst, 1u);
    EXPECT_EQ(pcb.remainingBurst, 1u);
    EXPECT_EQ(pcb.quantumUsed, 0u);
    EXPECT_EQ(pcb.ioBurstDuration, 0u);
    EXPECT_EQ(pcb.ioRemainingTicks, 0u);
    EXPECT_EQ(pcb.ioCompletionTick, 0u);
    EXPECT_EQ(pcb.memoryRequirement, 1u);
    EXPECT_EQ(pcb.pageTableId, -1);
    EXPECT_EQ(pcb.waitingTime, 0u);
    EXPECT_EQ(pcb.turnaroundTime, 0u);
    EXPECT_EQ(pcb.contextSwitches, 0u);
    EXPECT_EQ(pcb.pageFaultCount, 0u);
    EXPECT_TRUE(pcb.threadIds.empty());
}

TEST(PCBTest, InsertIntoProcessTable) {
    SimulationState state;

    PCB pcb;
    pcb.pid = state.nextPID++;
    pcb.name = "test_proc";
    pcb.type = ProcessType::IO_BOUND;
    pcb.priority = 3;
    pcb.totalCpuBurst = 10;
    pcb.remainingBurst = 10;
    pcb.arrivalTick = state.currentTick;

    state.processTable[pcb.pid] = pcb;

    EXPECT_EQ(state.processTable.size(), 1u);
    EXPECT_EQ(state.processTable[1].pid, 1);
    EXPECT_EQ(state.processTable[1].name, "test_proc");
    EXPECT_EQ(state.processTable[1].type, ProcessType::IO_BOUND);
    EXPECT_EQ(state.processTable[1].priority, 3);
    EXPECT_EQ(state.nextPID, 2);
}

// ══════════════════════════════════════════════════════════════
// TCB Tests
// ══════════════════════════════════════════════════════════════

TEST(TCBTest, DefaultValues) {
    TCB tcb;

    EXPECT_EQ(tcb.tid, -1);
    EXPECT_EQ(tcb.parentPid, -1);
    EXPECT_EQ(tcb.state, ThreadState::T_NEW);
    EXPECT_EQ(tcb.creationTick, 0u);
    EXPECT_EQ(tcb.stackSize, 2u);
    EXPECT_EQ(tcb.simulatedSP, 0u);
    EXPECT_EQ(tcb.cpuBurst, 1u);
    EXPECT_EQ(tcb.remainingBurst, 1u);
    EXPECT_EQ(tcb.blockedOnSyncId, -1);
    EXPECT_EQ(tcb.waitingTime, 0u);
}

TEST(TCBTest, InsertIntoThreadTable) {
    SimulationState state;

    TCB tcb;
    tcb.tid = state.nextTID++;
    tcb.parentPid = 1;
    tcb.state = ThreadState::T_RUNNABLE;
    tcb.cpuBurst = 5;
    tcb.remainingBurst = 5;

    state.threadTable[tcb.tid] = tcb;

    EXPECT_EQ(state.threadTable.size(), 1u);
    EXPECT_EQ(state.threadTable[1].parentPid, 1);
    EXPECT_EQ(state.threadTable[1].state, ThreadState::T_RUNNABLE);
}

// ══════════════════════════════════════════════════════════════
// FrameTable Tests
// ══════════════════════════════════════════════════════════════

TEST(FrameTableTest, DefaultEntryValues) {
    FrameTableEntry entry;

    EXPECT_EQ(entry.frameNumber, 0);
    EXPECT_FALSE(entry.occupied);
    EXPECT_EQ(entry.ownerPid, -1);
    EXPECT_EQ(entry.virtualPageNumber, 0u);
    EXPECT_EQ(entry.loadTick, 0u);
    EXPECT_EQ(entry.lastAccessTick, 0u);
}

TEST(FrameTableTest, CreateFrameTable) {
    SimulationState state;

    // Create 4 frames (minimum per DD)
    for (int i = 0; i < 4; i++) {
        FrameTableEntry entry;
        entry.frameNumber = i;
        state.frameTable.push_back(entry);
    }

    EXPECT_EQ(state.frameTable.size(), 4u);
    EXPECT_EQ(state.frameTable[0].frameNumber, 0);
    EXPECT_EQ(state.frameTable[3].frameNumber, 3);
    EXPECT_FALSE(state.frameTable[2].occupied);
}

// ══════════════════════════════════════════════════════════════
// PageTable Tests
// ══════════════════════════════════════════════════════════════

TEST(PageTableTest, DefaultValues) {
    PageTable pt;

    EXPECT_EQ(pt.ownerPid, -1);
    EXPECT_EQ(pt.pageSize, 256u);
    EXPECT_TRUE(pt.entries.empty());
}

TEST(PageTableTest, CreateWithEntries) {
    PageTable pt;
    pt.ownerPid = 1;

    // Create 4 page table entries
    for (uint32_t i = 0; i < 4; i++) {
        PageTableEntry pte;
        pte.virtualPageNumber = i;
        pt.entries.push_back(pte);
    }

    EXPECT_EQ(pt.entries.size(), 4u);
    EXPECT_EQ(pt.entries[2].virtualPageNumber, 2u);
    EXPECT_FALSE(pt.entries[0].valid);
    EXPECT_EQ(pt.entries[0].frameNumber, -1);
}

// ══════════════════════════════════════════════════════════════
// Reset Tests
// ══════════════════════════════════════════════════════════════

TEST(SimulationStateTest, ResetClearsAllState) {
    SimulationState state;

    // Populate state with data
    state.currentTick = 42;
    state.status = SimStatus::RUNNING;
    state.mode = SimMode::AUTO;
    state.nextPID = 5;
    state.runningPID = 3;
    state.activePolicy = "ROUND_ROBIN";
    state.timeQuantum = 4;
    state.activeReplacement = "LRU";

    PCB pcb;
    pcb.pid = 1;
    state.processTable[1] = pcb;

    state.readyQueue.push_back(2);
    state.ganttLog.push_back(GanttEntry(1, 1, "FCFS"));
    state.decisionLog.push_back(DecisionLogEntry(1, "test"));

    // Reset
    state.reset();

    // Verify all back to defaults
    EXPECT_EQ(state.currentTick, 0u);
    EXPECT_EQ(state.status, SimStatus::IDLE);
    EXPECT_EQ(state.mode, SimMode::STEP);
    EXPECT_EQ(state.nextPID, 1);
    EXPECT_EQ(state.nextTID, 1);
    EXPECT_EQ(state.runningPID, -1);
    EXPECT_EQ(state.activePolicy, "FCFS");
    EXPECT_EQ(state.timeQuantum, 2u);
    EXPECT_EQ(state.activeReplacement, "FIFO");
    EXPECT_TRUE(state.processTable.empty());
    EXPECT_TRUE(state.readyQueue.empty());
    EXPECT_TRUE(state.ganttLog.empty());
    EXPECT_TRUE(state.decisionLog.empty());
}

// ══════════════════════════════════════════════════════════════
// Enum toString Tests
// ══════════════════════════════════════════════════════════════

TEST(EnumTest, ProcessStateToString) {
    EXPECT_EQ(toString(ProcessState::NEW), "NEW");
    EXPECT_EQ(toString(ProcessState::READY), "READY");
    EXPECT_EQ(toString(ProcessState::RUNNING), "RUNNING");
    EXPECT_EQ(toString(ProcessState::WAITING), "WAITING");
    EXPECT_EQ(toString(ProcessState::TERMINATED), "TERMINATED");
}

TEST(EnumTest, ProcessStateFromString) {
    EXPECT_EQ(processStateFromString("NEW"), ProcessState::NEW);
    EXPECT_EQ(processStateFromString("RUNNING"), ProcessState::RUNNING);
    EXPECT_THROW(processStateFromString("INVALID"), std::invalid_argument);
}

TEST(EnumTest, SimModeRoundTrip) {
    EXPECT_EQ(simModeFromString(toString(SimMode::STEP)), SimMode::STEP);
    EXPECT_EQ(simModeFromString(toString(SimMode::AUTO)), SimMode::AUTO);
}

TEST(EnumTest, SimStatusRoundTrip) {
    EXPECT_EQ(simStatusFromString(toString(SimStatus::IDLE)), SimStatus::IDLE);
    EXPECT_EQ(simStatusFromString(toString(SimStatus::RUNNING)), SimStatus::RUNNING);
    EXPECT_EQ(simStatusFromString(toString(SimStatus::PAUSED)), SimStatus::PAUSED);
    EXPECT_EQ(simStatusFromString(toString(SimStatus::STOPPED)), SimStatus::STOPPED);
}

TEST(EnumTest, SchedulingPolicyRoundTrip) {
    EXPECT_EQ(schedulingPolicyFromString(toString(SchedulingPolicy::FCFS)), SchedulingPolicy::FCFS);
    EXPECT_EQ(schedulingPolicyFromString(toString(SchedulingPolicy::ROUND_ROBIN)), SchedulingPolicy::ROUND_ROBIN);
    EXPECT_EQ(schedulingPolicyFromString(toString(SchedulingPolicy::PRIORITY_NP)), SchedulingPolicy::PRIORITY_NP);
    EXPECT_EQ(schedulingPolicyFromString(toString(SchedulingPolicy::PRIORITY_P)), SchedulingPolicy::PRIORITY_P);
}

TEST(EnumTest, ReplacementPolicyRoundTrip) {
    EXPECT_EQ(replacementPolicyFromString(toString(ReplacementPolicy::FIFO)), ReplacementPolicy::FIFO);
    EXPECT_EQ(replacementPolicyFromString(toString(ReplacementPolicy::LRU)), ReplacementPolicy::LRU);
}

TEST(EnumTest, ProcessTypeRoundTrip) {
    EXPECT_EQ(processTypeFromString(toString(ProcessType::CPU_BOUND)), ProcessType::CPU_BOUND);
    EXPECT_EQ(processTypeFromString(toString(ProcessType::IO_BOUND)), ProcessType::IO_BOUND);
    EXPECT_EQ(processTypeFromString(toString(ProcessType::MIXED)), ProcessType::MIXED);
}

TEST(EnumTest, SyncPrimitiveTypeRoundTrip) {
    EXPECT_EQ(syncPrimitiveTypeFromString(toString(SyncPrimitiveType::MUTEX)), SyncPrimitiveType::MUTEX);
    EXPECT_EQ(syncPrimitiveTypeFromString(toString(SyncPrimitiveType::SEMAPHORE_BINARY)), SyncPrimitiveType::SEMAPHORE_BINARY);
    EXPECT_EQ(syncPrimitiveTypeFromString(toString(SyncPrimitiveType::SEMAPHORE_COUNTING)), SyncPrimitiveType::SEMAPHORE_COUNTING);
}
