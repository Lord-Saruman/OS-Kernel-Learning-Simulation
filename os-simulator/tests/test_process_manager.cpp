/**
 * test_process_manager.cpp — Unit Tests for the Process Manager Module
 *
 * Reference: Implementation Plan Phase 2
 *
 * Tests cover:
 *   Group 1: Module interface (name, status, reset)
 *   Group 2: Process creation (PID, names, auto-assign, events)
 *   Group 3: State transitions NEW → READY (admission)
 *   Group 4: I/O completion WAITING → READY
 *   Group 5: Kill process (* → TERMINATED)
 *   Group 6: Thread management
 *   Group 7: Waiting time accumulation
 *   Group 8: Edge cases
 */

#include <gtest/gtest.h>
#include <algorithm>

#include "core/SimulationState.h"
#include "core/EventBus.h"
#include "core/SimEnums.h"
#include "core/SimEvent.h"
#include "modules/process/ProcessManager.h"
#include "modules/process/ProcessSpec.h"
#include "modules/process/PCB.h"
#include "modules/process/TCB.h"

// ═══════════════════════════════════════════════════════════════
// Test Fixture
// ═══════════════════════════════════════════════════════════════

class ProcessManagerTest : public ::testing::Test {
protected:
    SimulationState state;
    EventBus bus;
    ProcessManager pm;

    void SetUp() override {
        // State and bus are default-constructed (Phase 1 verified)
        // Set simulation to running so onTick can be called
        state.status = SimStatus::RUNNING;
    }

    // Helper: create a simple CPU-bound spec
    ProcessSpec makeCpuSpec(const std::string& name = "", int burst = 10) {
        ProcessSpec spec;
        spec.name = name;
        spec.type = ProcessType::CPU_BOUND;
        spec.priority = 5;
        spec.cpuBurst = burst;
        spec.ioBurstDuration = 0;
        spec.memoryRequirement = 2;
        return spec;
    }

    // Helper: create an I/O-bound spec
    ProcessSpec makeIOSpec(const std::string& name = "", int burst = 4, int io = 5) {
        ProcessSpec spec;
        spec.name = name;
        spec.type = ProcessType::IO_BOUND;
        spec.priority = 3;
        spec.cpuBurst = burst;
        spec.ioBurstDuration = io;
        spec.memoryRequirement = 2;
        return spec;
    }

    // Helper: count events of a given type
    int countEvents(const std::string& eventType) {
        int count = 0;
        for (const auto& e : bus.getTickEvents()) {
            if (e.eventType == eventType) count++;
        }
        return count;
    }
};

// ═══════════════════════════════════════════════════════════════
// Group 1 — Module Interface
// ═══════════════════════════════════════════════════════════════

TEST_F(ProcessManagerTest, ModuleName) {
    EXPECT_EQ(pm.getModuleName(), "ProcessManager");
}

TEST_F(ProcessManagerTest, InitialStatus) {
    EXPECT_EQ(pm.getStatus(), ModuleStatus::IDLE);
}

TEST_F(ProcessManagerTest, StatusAfterTick) {
    pm.onTick(state, bus);
    EXPECT_EQ(pm.getStatus(), ModuleStatus::ACTIVE);
}

TEST_F(ProcessManagerTest, ResetClearsStatus) {
    pm.onTick(state, bus);
    EXPECT_EQ(pm.getStatus(), ModuleStatus::ACTIVE);
    pm.reset();
    EXPECT_EQ(pm.getStatus(), ModuleStatus::IDLE);
}

// ═══════════════════════════════════════════════════════════════
// Group 2 — Process Creation
// ═══════════════════════════════════════════════════════════════

TEST_F(ProcessManagerTest, CreateSingleProcess) {
    ProcessSpec spec = makeCpuSpec("test_proc", 10);
    int pid = pm.createProcess(state, bus, spec);

    EXPECT_EQ(pid, 1);
    ASSERT_TRUE(state.processTable.count(pid));

    const PCB& pcb = state.processTable[pid];
    EXPECT_EQ(pcb.pid, 1);
    EXPECT_EQ(pcb.name, "test_proc");
    EXPECT_EQ(pcb.type, ProcessType::CPU_BOUND);
    EXPECT_EQ(pcb.priority, 5);
    EXPECT_EQ(pcb.totalCpuBurst, 10u);
    EXPECT_EQ(pcb.remainingBurst, 10u);
    EXPECT_EQ(pcb.memoryRequirement, 2u);
}

TEST_F(ProcessManagerTest, PIDAutoIncrement) {
    int pid1 = pm.createProcess(state, bus, makeCpuSpec("p1"));
    int pid2 = pm.createProcess(state, bus, makeCpuSpec("p2"));
    int pid3 = pm.createProcess(state, bus, makeCpuSpec("p3"));

    EXPECT_EQ(pid1, 1);
    EXPECT_EQ(pid2, 2);
    EXPECT_EQ(pid3, 3);
}

TEST_F(ProcessManagerTest, AutoName) {
    ProcessSpec spec;
    spec.type = ProcessType::CPU_BOUND;
    spec.cpuBurst = 5;
    spec.memoryRequirement = 1;
    // name is empty — should auto-generate

    int pid = pm.createProcess(state, bus, spec);
    EXPECT_EQ(state.processTable[pid].name, "proc_1");

    int pid2 = pm.createProcess(state, bus, spec);
    EXPECT_EQ(state.processTable[pid2].name, "proc_2");
}

TEST_F(ProcessManagerTest, CustomName) {
    ProcessSpec spec = makeCpuSpec("my_process");
    int pid = pm.createProcess(state, bus, spec);
    EXPECT_EQ(state.processTable[pid].name, "my_process");
}

TEST_F(ProcessManagerTest, AutoAssignBurst_CPUBound) {
    ProcessSpec spec;
    spec.type = ProcessType::CPU_BOUND;
    spec.cpuBurst = 0;  // auto-assign
    spec.memoryRequirement = 1;

    int pid = pm.createProcess(state, bus, spec);
    uint32_t burst = state.processTable[pid].totalCpuBurst;
    EXPECT_GE(burst, 8u);
    EXPECT_LE(burst, 20u);
}

TEST_F(ProcessManagerTest, AutoAssignBurst_IOBound) {
    ProcessSpec spec;
    spec.type = ProcessType::IO_BOUND;
    spec.cpuBurst = 0;  // auto-assign
    spec.memoryRequirement = 1;

    int pid = pm.createProcess(state, bus, spec);
    uint32_t burst = state.processTable[pid].totalCpuBurst;
    EXPECT_GE(burst, 2u);
    EXPECT_LE(burst, 6u);
}

TEST_F(ProcessManagerTest, AutoAssignIO_CPUBound) {
    ProcessSpec spec;
    spec.type = ProcessType::CPU_BOUND;
    spec.cpuBurst = 10;
    spec.ioBurstDuration = 0;  // auto-assign → should be 0 for CPU_BOUND
    spec.memoryRequirement = 1;

    int pid = pm.createProcess(state, bus, spec);
    EXPECT_EQ(state.processTable[pid].ioBurstDuration, 0u);
}

TEST_F(ProcessManagerTest, AutoAssignMemory) {
    ProcessSpec spec;
    spec.type = ProcessType::CPU_BOUND;
    spec.cpuBurst = 10;
    spec.memoryRequirement = 0;  // auto-assign

    int pid = pm.createProcess(state, bus, spec);
    uint32_t mem = state.processTable[pid].memoryRequirement;
    EXPECT_GE(mem, 2u);
    EXPECT_LE(mem, 4u);
}

TEST_F(ProcessManagerTest, InitialState_NEW) {
    int pid = pm.createProcess(state, bus, makeCpuSpec("p1"));
    EXPECT_EQ(state.processTable[pid].state, ProcessState::NEW);
}

TEST_F(ProcessManagerTest, ArrivalTick) {
    state.currentTick = 42;
    int pid = pm.createProcess(state, bus, makeCpuSpec("p1"));
    EXPECT_EQ(state.processTable[pid].arrivalTick, 42u);
}

TEST_F(ProcessManagerTest, CreationEvent) {
    pm.createProcess(state, bus, makeCpuSpec("p1"));
    EXPECT_EQ(countEvents(EventTypes::PROCESS_CREATED), 1);
}

// ═══════════════════════════════════════════════════════════════
// Group 3 — State Transitions (NEW → READY)
// ═══════════════════════════════════════════════════════════════

TEST_F(ProcessManagerTest, AdmitOnTick) {
    int pid = pm.createProcess(state, bus, makeCpuSpec("p1"));
    EXPECT_EQ(state.processTable[pid].state, ProcessState::NEW);

    bus.clearTickEvents();
    pm.onTick(state, bus);
    EXPECT_EQ(state.processTable[pid].state, ProcessState::READY);
}

TEST_F(ProcessManagerTest, AddedToReadyQueue) {
    int pid = pm.createProcess(state, bus, makeCpuSpec("p1"));
    EXPECT_TRUE(state.readyQueue.empty());

    pm.onTick(state, bus);
    ASSERT_EQ(state.readyQueue.size(), 1u);
    EXPECT_EQ(state.readyQueue.front(), pid);
}

TEST_F(ProcessManagerTest, MultipleAdmit) {
    pm.createProcess(state, bus, makeCpuSpec("p1"));
    pm.createProcess(state, bus, makeCpuSpec("p2"));
    pm.createProcess(state, bus, makeCpuSpec("p3"));

    pm.onTick(state, bus);

    EXPECT_EQ(state.readyQueue.size(), 3u);
    for (auto& [pid, pcb] : state.processTable) {
        EXPECT_EQ(pcb.state, ProcessState::READY);
    }
}

TEST_F(ProcessManagerTest, PageTableIdAssigned) {
    int pid = pm.createProcess(state, bus, makeCpuSpec("p1"));
    EXPECT_EQ(state.processTable[pid].pageTableId, -1);

    pm.onTick(state, bus);
    EXPECT_EQ(state.processTable[pid].pageTableId, pid);
}

TEST_F(ProcessManagerTest, StateChangeEvent) {
    pm.createProcess(state, bus, makeCpuSpec("p1"));
    bus.clearTickEvents();

    pm.onTick(state, bus);
    EXPECT_GE(countEvents(EventTypes::PROCESS_STATE_CHANGED), 1);
}

TEST_F(ProcessManagerTest, DecisionLog) {
    pm.createProcess(state, bus, makeCpuSpec("p1"));
    state.decisionLog.clear();

    pm.onTick(state, bus);
    EXPECT_FALSE(state.decisionLog.empty());

    bool found = false;
    for (const auto& entry : state.decisionLog) {
        if (entry.message.find("admitted") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

// ═══════════════════════════════════════════════════════════════
// Group 4 — I/O Completion (WAITING → READY)
// ═══════════════════════════════════════════════════════════════

TEST_F(ProcessManagerTest, IOCountdown) {
    int pid = pm.createProcess(state, bus, makeIOSpec("io_proc", 4, 5));
    pm.onTick(state, bus);  // NEW → READY

    // Manually set process to WAITING with I/O remaining
    state.processTable[pid].state = ProcessState::WAITING;
    state.processTable[pid].ioRemainingTicks = 3;
    // Remove from ready queue since it's now waiting
    state.readyQueue.clear();

    bus.clearTickEvents();
    pm.onTick(state, bus);  // tick — ioRemaining: 3 → 2
    EXPECT_EQ(state.processTable[pid].ioRemainingTicks, 2u);
    EXPECT_EQ(state.processTable[pid].state, ProcessState::WAITING);
}

TEST_F(ProcessManagerTest, IOComplete) {
    int pid = pm.createProcess(state, bus, makeIOSpec("io_proc", 4, 5));
    pm.onTick(state, bus);  // NEW → READY

    // Set to WAITING with 1 tick remaining
    state.processTable[pid].state = ProcessState::WAITING;
    state.processTable[pid].ioRemainingTicks = 1;
    state.readyQueue.clear();

    bus.clearTickEvents();
    pm.onTick(state, bus);  // tick — ioRemaining: 1 → 0, WAITING → READY
    EXPECT_EQ(state.processTable[pid].ioRemainingTicks, 0u);
    EXPECT_EQ(state.processTable[pid].state, ProcessState::READY);
}

TEST_F(ProcessManagerTest, IOComplete_ReadyQueue) {
    int pid = pm.createProcess(state, bus, makeIOSpec("io_proc", 4, 5));
    pm.onTick(state, bus);  // NEW → READY

    state.processTable[pid].state = ProcessState::WAITING;
    state.processTable[pid].ioRemainingTicks = 1;
    state.readyQueue.clear();

    pm.onTick(state, bus);
    ASSERT_EQ(state.readyQueue.size(), 1u);
    EXPECT_EQ(state.readyQueue.back(), pid);
}

TEST_F(ProcessManagerTest, IOComplete_ResetFields) {
    int pid = pm.createProcess(state, bus, makeIOSpec("io_proc", 4, 5));
    pm.onTick(state, bus);

    state.processTable[pid].state = ProcessState::WAITING;
    state.processTable[pid].ioRemainingTicks = 1;
    state.processTable[pid].ioCompletionTick = 100;
    state.readyQueue.clear();

    pm.onTick(state, bus);
    EXPECT_EQ(state.processTable[pid].ioCompletionTick, 0u);
}

// ═══════════════════════════════════════════════════════════════
// Group 5 — Kill Process
// ═══════════════════════════════════════════════════════════════

TEST_F(ProcessManagerTest, KillReadyProcess) {
    int pid = pm.createProcess(state, bus, makeCpuSpec("p1"));
    pm.onTick(state, bus);  // NEW → READY
    EXPECT_EQ(state.processTable[pid].state, ProcessState::READY);

    bus.clearTickEvents();
    pm.killProcess(state, bus, pid);
    EXPECT_EQ(state.processTable[pid].state, ProcessState::TERMINATED);
}

TEST_F(ProcessManagerTest, KillRunningProcess) {
    int pid = pm.createProcess(state, bus, makeCpuSpec("p1"));
    pm.onTick(state, bus);  // NEW → READY

    // Simulate scheduler selecting this process
    state.processTable[pid].state = ProcessState::RUNNING;
    state.runningPID = pid;
    state.readyQueue.clear();

    pm.killProcess(state, bus, pid);
    EXPECT_EQ(state.processTable[pid].state, ProcessState::TERMINATED);
    EXPECT_EQ(state.runningPID, -1);
}

TEST_F(ProcessManagerTest, KillSetsTerminationTick) {
    state.currentTick = 10;
    int pid = pm.createProcess(state, bus, makeCpuSpec("p1"));
    pm.onTick(state, bus);

    state.currentTick = 25;
    pm.killProcess(state, bus, pid);
    EXPECT_EQ(state.processTable[pid].terminationTick, 25u);
}

TEST_F(ProcessManagerTest, KillComputesTurnaround) {
    state.currentTick = 5;
    int pid = pm.createProcess(state, bus, makeCpuSpec("p1"));
    pm.onTick(state, bus);

    state.currentTick = 15;
    pm.killProcess(state, bus, pid);
    EXPECT_EQ(state.processTable[pid].turnaroundTime, 10u);  // 15 - 5
}

TEST_F(ProcessManagerTest, KillRemovesFromReadyQueue) {
    int pid = pm.createProcess(state, bus, makeCpuSpec("p1"));
    pm.onTick(state, bus);
    EXPECT_EQ(state.readyQueue.size(), 1u);

    pm.killProcess(state, bus, pid);
    EXPECT_TRUE(state.readyQueue.empty());
}

TEST_F(ProcessManagerTest, KillTerminatesThreads) {
    int pid = pm.createProcess(state, bus, makeCpuSpec("p1"));
    pm.onTick(state, bus);

    int tid1 = pm.createThread(state, bus, pid, 3, 2);
    int tid2 = pm.createThread(state, bus, pid, 5, 2);

    pm.killProcess(state, bus, pid);
    EXPECT_EQ(state.threadTable[tid1].state, ThreadState::T_TERMINATED);
    EXPECT_EQ(state.threadTable[tid2].state, ThreadState::T_TERMINATED);
}

TEST_F(ProcessManagerTest, KillPublishesEvent) {
    int pid = pm.createProcess(state, bus, makeCpuSpec("p1"));
    pm.onTick(state, bus);

    bus.clearTickEvents();
    pm.killProcess(state, bus, pid);
    EXPECT_EQ(countEvents(EventTypes::PROCESS_TERMINATED), 1);
}

TEST_F(ProcessManagerTest, KillNonexistent) {
    // Should not crash or throw
    EXPECT_NO_THROW(pm.killProcess(state, bus, 999));
}

TEST_F(ProcessManagerTest, KillAlreadyTerminated) {
    int pid = pm.createProcess(state, bus, makeCpuSpec("p1"));
    pm.onTick(state, bus);

    pm.killProcess(state, bus, pid);
    bus.clearTickEvents();

    // Kill again — should be no-op
    pm.killProcess(state, bus, pid);
    EXPECT_EQ(countEvents(EventTypes::PROCESS_TERMINATED), 0);
}

// ═══════════════════════════════════════════════════════════════
// Group 6 — Thread Management
// ═══════════════════════════════════════════════════════════════

TEST_F(ProcessManagerTest, CreateThread) {
    int pid = pm.createProcess(state, bus, makeCpuSpec("p1"));
    pm.onTick(state, bus);

    int tid = pm.createThread(state, bus, pid, 5, 3);
    EXPECT_GE(tid, 1);
    ASSERT_TRUE(state.threadTable.count(tid));

    const TCB& tcb = state.threadTable[tid];
    EXPECT_EQ(tcb.tid, tid);
    EXPECT_EQ(tcb.parentPid, pid);
    EXPECT_EQ(tcb.state, ThreadState::T_NEW);
    EXPECT_EQ(tcb.cpuBurst, 5u);
    EXPECT_EQ(tcb.remainingBurst, 5u);
    EXPECT_EQ(tcb.stackSize, 3u);
    EXPECT_EQ(tcb.blockedOnSyncId, -1);
}

TEST_F(ProcessManagerTest, TIDAutoIncrement) {
    int pid = pm.createProcess(state, bus, makeCpuSpec("p1"));
    pm.onTick(state, bus);

    int tid1 = pm.createThread(state, bus, pid);
    int tid2 = pm.createThread(state, bus, pid);
    int tid3 = pm.createThread(state, bus, pid);
    EXPECT_EQ(tid1, 1);
    EXPECT_EQ(tid2, 2);
    EXPECT_EQ(tid3, 3);
}

TEST_F(ProcessManagerTest, ThreadLinkedToParent) {
    int pid = pm.createProcess(state, bus, makeCpuSpec("p1"));
    pm.onTick(state, bus);

    int tid = pm.createThread(state, bus, pid);
    const auto& threadIds = state.processTable[pid].threadIds;
    EXPECT_TRUE(std::find(threadIds.begin(), threadIds.end(), tid) != threadIds.end());
}

TEST_F(ProcessManagerTest, ThreadInitialState) {
    int pid = pm.createProcess(state, bus, makeCpuSpec("p1"));
    pm.onTick(state, bus);

    int tid = pm.createThread(state, bus, pid);
    EXPECT_EQ(state.threadTable[tid].state, ThreadState::T_NEW);
}

TEST_F(ProcessManagerTest, CreateThreadInvalidParent) {
    int tid = pm.createThread(state, bus, 999);  // non-existent PID
    EXPECT_EQ(tid, -1);
}

TEST_F(ProcessManagerTest, CreateThreadTerminatedParent) {
    int pid = pm.createProcess(state, bus, makeCpuSpec("p1"));
    pm.onTick(state, bus);
    pm.killProcess(state, bus, pid);

    int tid = pm.createThread(state, bus, pid);
    EXPECT_EQ(tid, -1);
}

// ═══════════════════════════════════════════════════════════════
// Group 7 — Waiting Time Accumulation
// ═══════════════════════════════════════════════════════════════

TEST_F(ProcessManagerTest, WaitingTimeIncrements) {
    int pid = pm.createProcess(state, bus, makeCpuSpec("p1"));
    pm.onTick(state, bus);  // NEW → READY, +1 waitingTime (first tick in READY)
    EXPECT_EQ(state.processTable[pid].waitingTime, 1u);

    pm.onTick(state, bus);  // +1 more
    EXPECT_EQ(state.processTable[pid].waitingTime, 2u);

    pm.onTick(state, bus);  // +1 more
    EXPECT_EQ(state.processTable[pid].waitingTime, 3u);
}

TEST_F(ProcessManagerTest, WaitingTimeNotInRunning) {
    int pid = pm.createProcess(state, bus, makeCpuSpec("p1"));
    pm.onTick(state, bus);  // NEW → READY, +1

    // Simulate scheduler dispatching process
    state.processTable[pid].state = ProcessState::RUNNING;
    state.runningPID = pid;
    state.readyQueue.clear();

    uint64_t before = state.processTable[pid].waitingTime;
    pm.onTick(state, bus);
    EXPECT_EQ(state.processTable[pid].waitingTime, before);  // unchanged
}

TEST_F(ProcessManagerTest, WaitingTimeMultiple) {
    int pid1 = pm.createProcess(state, bus, makeCpuSpec("p1"));
    int pid2 = pm.createProcess(state, bus, makeCpuSpec("p2"));

    pm.onTick(state, bus);  // Both admitted
    EXPECT_EQ(state.processTable[pid1].waitingTime, 1u);
    EXPECT_EQ(state.processTable[pid2].waitingTime, 1u);

    pm.onTick(state, bus);
    EXPECT_EQ(state.processTable[pid1].waitingTime, 2u);
    EXPECT_EQ(state.processTable[pid2].waitingTime, 2u);
}

// ═══════════════════════════════════════════════════════════════
// Group 8 — Edge Cases
// ═══════════════════════════════════════════════════════════════

TEST_F(ProcessManagerTest, NoProcessesNoError) {
    // onTick with empty process table should not crash
    EXPECT_NO_THROW(pm.onTick(state, bus));
}

TEST_F(ProcessManagerTest, MaxPriority) {
    ProcessSpec spec = makeCpuSpec("high_prio");
    spec.priority = 1;
    int pid = pm.createProcess(state, bus, spec);
    EXPECT_EQ(state.processTable[pid].priority, 1);
}

TEST_F(ProcessManagerTest, MinPriority) {
    ProcessSpec spec = makeCpuSpec("low_prio");
    spec.priority = 10;
    int pid = pm.createProcess(state, bus, spec);
    EXPECT_EQ(state.processTable[pid].priority, 10);
}

TEST_F(ProcessManagerTest, ZeroBurstAutoAssign) {
    ProcessSpec spec;
    spec.type = ProcessType::MIXED;
    spec.cpuBurst = 0;
    spec.memoryRequirement = 1;

    int pid = pm.createProcess(state, bus, spec);
    EXPECT_GE(state.processTable[pid].totalCpuBurst, 1u);
    EXPECT_EQ(state.processTable[pid].totalCpuBurst,
              state.processTable[pid].remainingBurst);
}

TEST_F(ProcessManagerTest, ProcessPersistsAfterTermination) {
    int pid = pm.createProcess(state, bus, makeCpuSpec("p1"));
    pm.onTick(state, bus);
    pm.killProcess(state, bus, pid);

    // Process should still be in processTable (persists until reset)
    EXPECT_TRUE(state.processTable.count(pid));
    EXPECT_EQ(state.processTable[pid].state, ProcessState::TERMINATED);

    // Run several more ticks — process should not be removed
    for (int i = 0; i < 10; i++) {
        pm.onTick(state, bus);
    }
    EXPECT_TRUE(state.processTable.count(pid));
}

TEST_F(ProcessManagerTest, MultipleIOProcesses) {
    int pid1 = pm.createProcess(state, bus, makeIOSpec("io1", 4, 5));
    int pid2 = pm.createProcess(state, bus, makeIOSpec("io2", 4, 5));
    pm.onTick(state, bus);  // Both NEW → READY

    // Set both to WAITING with different I/O times
    state.processTable[pid1].state = ProcessState::WAITING;
    state.processTable[pid1].ioRemainingTicks = 1;
    state.processTable[pid2].state = ProcessState::WAITING;
    state.processTable[pid2].ioRemainingTicks = 3;
    state.readyQueue.clear();

    pm.onTick(state, bus);
    // pid1 should complete I/O, pid2 should still be waiting
    EXPECT_EQ(state.processTable[pid1].state, ProcessState::READY);
    EXPECT_EQ(state.processTable[pid2].state, ProcessState::WAITING);
    EXPECT_EQ(state.processTable[pid2].ioRemainingTicks, 2u);
    EXPECT_EQ(state.readyQueue.size(), 1u);
}

TEST_F(ProcessManagerTest, ThreadCreationTick) {
    state.currentTick = 7;
    int pid = pm.createProcess(state, bus, makeCpuSpec("p1"));
    pm.onTick(state, bus);

    state.currentTick = 12;
    int tid = pm.createThread(state, bus, pid);
    EXPECT_EQ(state.threadTable[tid].creationTick, 12u);
}
