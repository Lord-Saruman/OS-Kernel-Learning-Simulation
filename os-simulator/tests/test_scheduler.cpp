/**
 * test_scheduler.cpp — CPU Scheduler Module Unit Tests
 *
 * Phase 3 test suite. Verifies correctness against hand-computed
 * textbook examples for FCFS, Round Robin, Priority (NP & P).
 *
 * Test naming convention: TEST(TestSuite, TestCase)
 *   - SchedulerFCFS_*        — FCFS policy tests
 *   - SchedulerRR_*          — Round Robin tests
 *   - SchedulerPriorityNP_*  — Priority non-preemptive tests
 *   - SchedulerPriorityP_*   — Priority preemptive tests
 *   - SchedulerGeneral_*     — Policy-agnostic behavior tests
 */

#include <gtest/gtest.h>
#include "core/SimulationState.h"
#include "core/EventBus.h"
#include "core/SimEnums.h"
#include "modules/process/ProcessManager.h"
#include "modules/process/ProcessSpec.h"
#include "modules/scheduler/Scheduler.h"

// ═══════════════════════════════════════════════════════════════
// Helper: Create a ProcessSpec with explicit fields
// ═══════════════════════════════════════════════════════════════

static ProcessSpec makeSpec(const std::string& name, ProcessType type,
                            int priority, uint32_t cpuBurst,
                            uint32_t ioBurst = 0, uint32_t cpuSegment = 0) {
    ProcessSpec spec;
    spec.name = name;
    spec.type = type;
    spec.priority = priority;
    spec.cpuBurst = cpuBurst;
    spec.ioBurstDuration = ioBurst;
    spec.memoryRequirement = 1;
    spec.cpuSegmentLength = cpuSegment;
    return spec;
}

// Helper: Run N ticks (ProcessManager first, then Scheduler)
static void runTicks(int n, SimulationState& state, EventBus& bus,
                     ProcessManager& pm, Scheduler& sched) {
    for (int i = 0; i < n; i++) {
        state.currentTick++;
        pm.onTick(state, bus);
        sched.onTick(state, bus);
    }
}

// Helper: Run single tick
static void runOneTick(SimulationState& state, EventBus& bus,
                       ProcessManager& pm, Scheduler& sched) {
    runTicks(1, state, bus, pm, sched);
}

// ═══════════════════════════════════════════════════════════════
// Test 1: FCFS — Basic Ordering (Textbook Example)
//
// | Process | Arrival | Burst |
// |---------|---------|-------|
// | P1      | 0 (t=1) | 4    |
// | P2      | 0 (t=1) | 3    |
// | P3      | 0 (t=1) | 2    |
//
// All arrive at tick 0 (created before ticks start).
// Expected Gantt: P1 P1 P1 P1 P2 P2 P2 P3 P3
// ═══════════════════════════════════════════════════════════════

TEST(SchedulerFCFS, BasicOrdering) {
    SimulationState state;
    EventBus bus;
    ProcessManager pm;
    Scheduler sched;
    sched.setPolicy("FCFS");

    // Create all processes at tick 0 (CPU_BOUND, no I/O)
    pm.createProcess(state, bus, makeSpec("P1", ProcessType::CPU_BOUND, 5, 4));
    pm.createProcess(state, bus, makeSpec("P2", ProcessType::CPU_BOUND, 5, 3));
    pm.createProcess(state, bus, makeSpec("P3", ProcessType::CPU_BOUND, 5, 2));

    // Run 9 ticks (4 + 3 + 2)
    runTicks(9, state, bus, pm, sched);

    // All processes should be TERMINATED
    EXPECT_EQ(state.processTable[1].state, ProcessState::TERMINATED);
    EXPECT_EQ(state.processTable[2].state, ProcessState::TERMINATED);
    EXPECT_EQ(state.processTable[3].state, ProcessState::TERMINATED);

    // Verify Gantt chart (9 entries for ticks 1-9)
    ASSERT_EQ(state.ganttLog.size(), 9u);

    // P1 runs ticks 1-4
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(state.ganttLog[i].pid, 1) << "Tick " << (i + 1);
    }
    // P2 runs ticks 5-7
    for (int i = 4; i < 7; i++) {
        EXPECT_EQ(state.ganttLog[i].pid, 2) << "Tick " << (i + 1);
    }
    // P3 runs ticks 8-9
    for (int i = 7; i < 9; i++) {
        EXPECT_EQ(state.ganttLog[i].pid, 3) << "Tick " << (i + 1);
    }

    // Verify metrics
    // P1: wait=0, turnaround=4  (terminates at tick 4, arrived tick 0)
    // P2: wait=4, turnaround=7  (terminates at tick 7, arrived tick 0)
    // P3: wait=7, turnaround=9  (terminates at tick 9, arrived tick 0)
    // BUT: arrival is at tick 0, but ticks start at 1. Let's check PCB values.
    EXPECT_EQ(state.processTable[1].terminationTick, 4u);
    EXPECT_EQ(state.processTable[2].terminationTick, 7u);
    EXPECT_EQ(state.processTable[3].terminationTick, 9u);

    // CPU utilization should be 100%
    EXPECT_FLOAT_EQ(state.metrics.cpuUtilization, 100.0f);
    EXPECT_EQ(state.metrics.completedProcesses, 3u);
}

// ═══════════════════════════════════════════════════════════════
// Test 2: Round Robin — Quantum = 2
//
// | Process | Arrival | Burst |
// |---------|---------|-------|
// | P1      | 0       | 5    |
// | P2      | 0       | 3    |
// | P3      | 0       | 1    |
//
// Expected execution with quantum=2:
// T1: P1 dispatched, burst-- → 4, qUsed=1
// T2: P1 continues, burst-- → 3, qUsed=2 → quantum expired, P1→back
// T3: P2 dispatched, burst-- → 2, qUsed=1
// T4: P2 continues, burst-- → 1, qUsed=2 → quantum expired, P2→back
// T5: P3 dispatched, burst-- → 0 → P3 TERMINATED
// T6: P1 dispatched, burst-- → 2, qUsed=1
// T7: P1 continues, burst-- → 1, qUsed=2 → quantum expired, P1→back
// T8: P2 dispatched, burst-- → 0 → P2 TERMINATED
// T9: P1 dispatched, burst-- → 0 → P1 TERMINATED
// ═══════════════════════════════════════════════════════════════

TEST(SchedulerRR, QuantumTwo) {
    SimulationState state;
    EventBus bus;
    ProcessManager pm;
    Scheduler sched;
    sched.setPolicy("ROUND_ROBIN");
    state.timeQuantum = 2;

    pm.createProcess(state, bus, makeSpec("P1", ProcessType::CPU_BOUND, 5, 5));
    pm.createProcess(state, bus, makeSpec("P2", ProcessType::CPU_BOUND, 5, 3));
    pm.createProcess(state, bus, makeSpec("P3", ProcessType::CPU_BOUND, 5, 1));

    // Run 9 ticks (total bursts: 5+3+1 = 9)
    runTicks(9, state, bus, pm, sched);

    // All terminated
    EXPECT_EQ(state.processTable[1].state, ProcessState::TERMINATED);
    EXPECT_EQ(state.processTable[2].state, ProcessState::TERMINATED);
    EXPECT_EQ(state.processTable[3].state, ProcessState::TERMINATED);

    // Verify Gantt: P1 P1 P2 P2 P3 P1 P1 P2 P1
    ASSERT_EQ(state.ganttLog.size(), 9u);
    EXPECT_EQ(state.ganttLog[0].pid, 1);  // T1: P1
    EXPECT_EQ(state.ganttLog[1].pid, 1);  // T2: P1
    EXPECT_EQ(state.ganttLog[2].pid, 2);  // T3: P2
    EXPECT_EQ(state.ganttLog[3].pid, 2);  // T4: P2
    EXPECT_EQ(state.ganttLog[4].pid, 3);  // T5: P3
    EXPECT_EQ(state.ganttLog[5].pid, 1);  // T6: P1
    EXPECT_EQ(state.ganttLog[6].pid, 1);  // T7: P1
    EXPECT_EQ(state.ganttLog[7].pid, 2);  // T8: P2
    EXPECT_EQ(state.ganttLog[8].pid, 1);  // T9: P1

    EXPECT_EQ(state.metrics.completedProcesses, 3u);
    EXPECT_FLOAT_EQ(state.metrics.cpuUtilization, 100.0f);
}

// ═══════════════════════════════════════════════════════════════
// Test 3: Round Robin — Quantum = 4
//
// | Process | Arrival | Burst |
// |---------|---------|-------|
// | P1      | 0       | 6    |
// | P2      | 0       | 4    |
// | P3      | 0       | 2    |
//
// Expected: P1(4) P2(4) P3(2) P1(2)
// T1-4: P1 runs 4 ticks → quantum expired
// T5-8: P2 runs 4 ticks → complete
// T9-10: P3 runs 2 ticks → complete
// T11-12: P1 runs 2 ticks → complete
// ═══════════════════════════════════════════════════════════════

TEST(SchedulerRR, QuantumFour) {
    SimulationState state;
    EventBus bus;
    ProcessManager pm;
    Scheduler sched;
    sched.setPolicy("ROUND_ROBIN");
    state.timeQuantum = 4;

    pm.createProcess(state, bus, makeSpec("P1", ProcessType::CPU_BOUND, 5, 6));
    pm.createProcess(state, bus, makeSpec("P2", ProcessType::CPU_BOUND, 5, 4));
    pm.createProcess(state, bus, makeSpec("P3", ProcessType::CPU_BOUND, 5, 2));

    runTicks(12, state, bus, pm, sched);

    EXPECT_EQ(state.processTable[1].state, ProcessState::TERMINATED);
    EXPECT_EQ(state.processTable[2].state, ProcessState::TERMINATED);
    EXPECT_EQ(state.processTable[3].state, ProcessState::TERMINATED);

    ASSERT_EQ(state.ganttLog.size(), 12u);
    // P1 runs ticks 1-4
    for (int i = 0; i < 4; i++) EXPECT_EQ(state.ganttLog[i].pid, 1);
    // P2 runs ticks 5-8
    for (int i = 4; i < 8; i++) EXPECT_EQ(state.ganttLog[i].pid, 2);
    // P3 runs ticks 9-10
    for (int i = 8; i < 10; i++) EXPECT_EQ(state.ganttLog[i].pid, 3);
    // P1 resumes ticks 11-12
    for (int i = 10; i < 12; i++) EXPECT_EQ(state.ganttLog[i].pid, 1);
}

// ═══════════════════════════════════════════════════════════════
// Test 4: Priority Non-Preemptive
//
// | Process | Arrival (tick) | Burst | Priority |
// |---------|----------------|-------|----------|
// | P1      | 0              | 6     | 3        |
// | P2      | 1              | 4     | 1 (best) |
// | P3      | 2              | 2     | 2        |
//
// Non-preemptive: P1 runs to completion (6 ticks), then P2 (4), then P3 (2)
// Total: 12 ticks
// ═══════════════════════════════════════════════════════════════

TEST(SchedulerPriorityNP, BasicOrdering) {
    SimulationState state;
    EventBus bus;
    ProcessManager pm;
    Scheduler sched;
    sched.setPolicy("PRIORITY_NP");

    // P1 arrives at tick 0
    pm.createProcess(state, bus, makeSpec("P1", ProcessType::CPU_BOUND, 3, 6));

    // Run 1 tick so P1 gets admitted and dispatched
    runOneTick(state, bus, pm, sched);  // tick 1

    // P2 arrives at tick 1 (will be admitted at tick 2)
    pm.createProcess(state, bus, makeSpec("P2", ProcessType::CPU_BOUND, 1, 4));

    runOneTick(state, bus, pm, sched);  // tick 2

    // P3 arrives at tick 2 (will be admitted at tick 3)
    pm.createProcess(state, bus, makeSpec("P3", ProcessType::CPU_BOUND, 2, 2));

    // Run remaining ticks
    runTicks(10, state, bus, pm, sched);  // ticks 3-12

    EXPECT_EQ(state.processTable[1].state, ProcessState::TERMINATED);
    EXPECT_EQ(state.processTable[2].state, ProcessState::TERMINATED);
    EXPECT_EQ(state.processTable[3].state, ProcessState::TERMINATED);

    // P1 should run first (was already running, non-preemptive)
    // Then P2 (priority 1, best), then P3 (priority 2)
    ASSERT_GE(state.ganttLog.size(), 12u);

    // P1 runs ticks 1-6
    for (int i = 0; i < 6; i++) {
        EXPECT_EQ(state.ganttLog[i].pid, 1) << "Tick " << (i + 1);
    }
    // P2 runs ticks 7-10
    for (int i = 6; i < 10; i++) {
        EXPECT_EQ(state.ganttLog[i].pid, 2) << "Tick " << (i + 1);
    }
    // P3 runs ticks 11-12
    for (int i = 10; i < 12; i++) {
        EXPECT_EQ(state.ganttLog[i].pid, 3) << "Tick " << (i + 1);
    }
}

// ═══════════════════════════════════════════════════════════════
// Test 5: Priority Preemptive
//
// | Process | Arrival (tick) | Burst | Priority |
// |---------|----------------|-------|----------|
// | P1      | 0              | 6     | 3        |
// | P2      | 2              | 4     | 1 (best) |
// | P3      | 4              | 2     | 2        |
//
// T1: P1 dispatched (only process), burst-- → 5
// T2: P1 continues, burst-- → 4.  P2 created.
// T3: P2 admitted. P2 preempts P1 (pri 1 < 3). P2 dispatched, burst-- → 3
// T4: P2 continues, burst-- → 2.  P3 created.
// T5: P3 admitted. P3 cannot preempt P2 (pri 2 > 1). P2 continues, burst-- → 1
// T6: P2 continues, burst-- → 0 → P2 TERMINATED
// T7: P3 dispatched (pri 2 < 3), burst-- → 1
// T8: P3 continues, burst-- → 0 → P3 TERMINATED
// T9: P1 dispatched (only one left), burst-- → 3
// T10-12: P1 continues to completion
// ═══════════════════════════════════════════════════════════════

TEST(SchedulerPriorityP, BasicPreemption) {
    SimulationState state;
    EventBus bus;
    ProcessManager pm;
    Scheduler sched;
    sched.setPolicy("PRIORITY_P");

    // P1 at tick 0
    pm.createProcess(state, bus, makeSpec("P1", ProcessType::CPU_BOUND, 3, 6));

    runOneTick(state, bus, pm, sched);  // T1: P1 dispatched
    EXPECT_EQ(state.ganttLog.back().pid, 1);

    runOneTick(state, bus, pm, sched);  // T2: P1 continues
    EXPECT_EQ(state.ganttLog.back().pid, 1);

    // P2 arrives at tick 2 (higher priority)
    pm.createProcess(state, bus, makeSpec("P2", ProcessType::CPU_BOUND, 1, 4));

    runOneTick(state, bus, pm, sched);  // T3: P2 preempts P1
    EXPECT_EQ(state.ganttLog.back().pid, 2);
    // P2 was dispatched and burst decremented, still has burst 3 → still running
    EXPECT_EQ(state.runningPID, 2);

    runOneTick(state, bus, pm, sched);  // T4: P2 continues
    EXPECT_EQ(state.ganttLog.back().pid, 2);

    // P3 arrives at tick 4
    pm.createProcess(state, bus, makeSpec("P3", ProcessType::CPU_BOUND, 2, 2));

    runOneTick(state, bus, pm, sched);  // T5: P2 continues (P3 can't preempt)
    EXPECT_EQ(state.ganttLog.back().pid, 2);

    runOneTick(state, bus, pm, sched);  // T6: P2 finishes
    EXPECT_EQ(state.processTable[2].state, ProcessState::TERMINATED);

    runOneTick(state, bus, pm, sched);  // T7: P3 dispatched (pri 2 < 3)
    EXPECT_EQ(state.ganttLog.back().pid, 3);

    runOneTick(state, bus, pm, sched);  // T8: P3 finishes
    EXPECT_EQ(state.processTable[3].state, ProcessState::TERMINATED);

    runOneTick(state, bus, pm, sched);  // T9: P1 resumes
    EXPECT_EQ(state.ganttLog.back().pid, 1);

    runTicks(3, state, bus, pm, sched);  // T10-12: P1 finishes
    EXPECT_EQ(state.processTable[1].state, ProcessState::TERMINATED);

    EXPECT_EQ(state.metrics.completedProcesses, 3u);
}

// ═══════════════════════════════════════════════════════════════
// Test 6: Policy Hot-Swap Mid-Simulation
// ═══════════════════════════════════════════════════════════════

TEST(SchedulerGeneral, PolicyHotSwap) {
    SimulationState state;
    EventBus bus;
    ProcessManager pm;
    Scheduler sched;
    sched.setPolicy("FCFS");

    pm.createProcess(state, bus, makeSpec("P1", ProcessType::CPU_BOUND, 5, 6));
    pm.createProcess(state, bus, makeSpec("P2", ProcessType::CPU_BOUND, 1, 4));

    // Run 3 ticks with FCFS
    runTicks(3, state, bus, pm, sched);

    // Verify FCFS: P1 should be running (arrived first)
    EXPECT_EQ(state.ganttLog[0].pid, 1);
    EXPECT_EQ(state.ganttLog[0].policySnapshot, "FCFS");

    // Hot-swap to Round Robin
    sched.setPolicy("ROUND_ROBIN");
    state.timeQuantum = 2;

    // Run more ticks
    runTicks(3, state, bus, pm, sched);

    // Verify policy name changed in Gantt entries
    EXPECT_EQ(sched.getActivePolicyName(), "ROUND_ROBIN");
    // The latest Gantt entries should show ROUND_ROBIN
    EXPECT_EQ(state.ganttLog.back().policySnapshot, "ROUND_ROBIN");
}

// ═══════════════════════════════════════════════════════════════
// Test 7: CPU Idle Handling
// ═══════════════════════════════════════════════════════════════

TEST(SchedulerGeneral, CPUIdle) {
    SimulationState state;
    EventBus bus;
    ProcessManager pm;
    Scheduler sched;

    // Run 3 ticks with no processes
    runTicks(3, state, bus, pm, sched);

    ASSERT_EQ(state.ganttLog.size(), 3u);

    // All entries should show idle (pid = -1)
    for (const auto& entry : state.ganttLog) {
        EXPECT_EQ(entry.pid, -1);
    }

    // CPU utilization should be 0%
    EXPECT_FLOAT_EQ(state.metrics.cpuUtilization, 0.0f);
}

// ═══════════════════════════════════════════════════════════════
// Test 8: Process Completion Updates Metrics Correctly
// ═══════════════════════════════════════════════════════════════

TEST(SchedulerGeneral, MetricsOnCompletion) {
    SimulationState state;
    EventBus bus;
    ProcessManager pm;
    Scheduler sched;
    sched.setPolicy("FCFS");

    pm.createProcess(state, bus, makeSpec("P1", ProcessType::CPU_BOUND, 5, 3));
    pm.createProcess(state, bus, makeSpec("P2", ProcessType::CPU_BOUND, 5, 2));

    // Run 5 ticks (3 + 2)
    runTicks(5, state, bus, pm, sched);

    EXPECT_EQ(state.metrics.completedProcesses, 2u);
    EXPECT_EQ(state.metrics.totalProcesses, 2u);
    EXPECT_FLOAT_EQ(state.metrics.cpuUtilization, 100.0f);

    // ProcessManager increments waitingTime each tick for READY processes,
    // including the tick they are admitted (NEW→READY). So:
    // T1: PM admits both → READY. PM increments wait for both (P1:+1, P2:+1).
    //     Scheduler dispatches P1. P2 stays READY.
    // T2: PM increments wait for P2 (+1). Scheduler continues P1.
    // T3: PM increments wait for P2 (+1). Scheduler finishes P1.
    // T4: PM increments wait for P2 (+1). Scheduler dispatches P2.
    // T5: PM has nothing to increment. Scheduler finishes P2.
    //
    // P1: waitingTime=1, turnaround=3
    // P2: waitingTime=4, turnaround=5
    // avg waiting = (1+4)/2 = 2.5
    // avg turnaround = (3+5)/2 = 4.0
    EXPECT_NEAR(state.metrics.avgWaitingTime, 2.5f, 0.1f);
    EXPECT_NEAR(state.metrics.avgTurnaroundTime, 4.0f, 0.1f);
}

// ═══════════════════════════════════════════════════════════════
// Test 9: Context Switch Counting
// ═══════════════════════════════════════════════════════════════

TEST(SchedulerGeneral, ContextSwitchCounting) {
    SimulationState state;
    EventBus bus;
    ProcessManager pm;
    Scheduler sched;
    sched.setPolicy("ROUND_ROBIN");
    state.timeQuantum = 2;

    pm.createProcess(state, bus, makeSpec("P1", ProcessType::CPU_BOUND, 5, 4));
    pm.createProcess(state, bus, makeSpec("P2", ProcessType::CPU_BOUND, 5, 4));

    runTicks(8, state, bus, pm, sched);

    // Both should be terminated
    EXPECT_EQ(state.processTable[1].state, ProcessState::TERMINATED);
    EXPECT_EQ(state.processTable[2].state, ProcessState::TERMINATED);

    // P1 and P2 should have context switches (preempted at quantum boundaries)
    EXPECT_GT(state.processTable[1].contextSwitches, 0u);
    EXPECT_GT(state.processTable[2].contextSwitches, 0u);

    // Total context switches should be tracked
    EXPECT_GT(state.metrics.totalContextSwitches, 0u);
}

// ═══════════════════════════════════════════════════════════════
// Test 10: Single Process — Edge Case
// ═══════════════════════════════════════════════════════════════

TEST(SchedulerGeneral, SingleProcess) {
    SimulationState state;
    EventBus bus;
    ProcessManager pm;
    Scheduler sched;
    sched.setPolicy("FCFS");

    pm.createProcess(state, bus, makeSpec("Solo", ProcessType::CPU_BOUND, 5, 3));

    runTicks(3, state, bus, pm, sched);

    EXPECT_EQ(state.processTable[1].state, ProcessState::TERMINATED);
    // PM increments waitingTime on the admission tick (T1) before scheduler
    // dispatches, so even a single process gets waitingTime=1
    EXPECT_EQ(state.processTable[1].waitingTime, 1u);
    EXPECT_EQ(state.processTable[1].turnaroundTime, 3u);
    EXPECT_EQ(state.metrics.completedProcesses, 1u);
    EXPECT_FLOAT_EQ(state.metrics.cpuUtilization, 100.0f);
}

// ═══════════════════════════════════════════════════════════════
// Test 11: I/O Burst Cycle — Deterministic cpuSegmentLength
// ═══════════════════════════════════════════════════════════════

TEST(SchedulerGeneral, IOBurstCycle) {
    SimulationState state;
    EventBus bus;
    ProcessManager pm;
    Scheduler sched;
    sched.setPolicy("FCFS");

    // Create a process with cpuSegmentLength=2, ioBurst=2, totalBurst=6
    // The I/O check fires at the START of a tick (Phase A), checking
    // quantumUsed which was set by the PREVIOUS tick's decrementBurst.
    //
    // Expected timeline:
    // T1: PM admits. Scheduler dispatches. Gantt=P1. Burst--→5, qUsed=1
    // T2: I/O check: qUsed(1) < segment(2) → no. Gantt=P1. Burst--→4, qUsed=2
    // T3: I/O check: qUsed(2) >= segment(2) → I/O! RUNNING→WAITING. CPU idle.
    //     PM handles I/O decrement. Gantt=-1.
    // T4: PM: I/O tick 2. Gantt=-1.
    // T5: PM: I/O complete → READY. Scheduler dispatches. Gantt=P1. Burst--→3, qUsed=1
    // T6: Gantt=P1. Burst--→2, qUsed=2
    // T7: I/O check: qUsed(2) >= segment(2) → I/O! Gantt=-1.
    // T8: PM: I/O tick. Gantt=-1.
    // T9: PM: I/O complete → READY. Dispatch. Gantt=P1. Burst--→1, qUsed=1
    // T10: Gantt=P1. Burst--→0 → TERMINATED
    ProcessSpec spec = makeSpec("IOProc", ProcessType::MIXED, 5, 6, 2, 2);
    pm.createProcess(state, bus, spec);

    // T1: admitted → dispatched, burst-- → 5, qUsed=1
    runOneTick(state, bus, pm, sched);
    EXPECT_EQ(state.runningPID, 1);
    EXPECT_EQ(state.processTable[1].remainingBurst, 5u);
    EXPECT_EQ(state.processTable[1].quantumUsed, 1u);

    // T2: still running, burst-- → 4, qUsed=2
    runOneTick(state, bus, pm, sched);
    EXPECT_EQ(state.runningPID, 1);
    EXPECT_EQ(state.processTable[1].remainingBurst, 4u);
    EXPECT_EQ(state.processTable[1].quantumUsed, 2u);

    // T3: I/O check fires (qUsed=2 >= segment=2) → WAITING
    runOneTick(state, bus, pm, sched);
    EXPECT_EQ(state.processTable[1].state, ProcessState::WAITING);
    EXPECT_EQ(state.runningPID, -1);

    // T4: I/O countdown
    runOneTick(state, bus, pm, sched);

    // T5: I/O complete → READY → dispatched
    runOneTick(state, bus, pm, sched);
    EXPECT_EQ(state.runningPID, 1);
    EXPECT_EQ(state.processTable[1].remainingBurst, 3u);

    // T6: continues
    runOneTick(state, bus, pm, sched);
    EXPECT_EQ(state.runningPID, 1);
    EXPECT_EQ(state.processTable[1].remainingBurst, 2u);

    // T7: I/O check fires again → WAITING
    runOneTick(state, bus, pm, sched);
    EXPECT_EQ(state.processTable[1].state, ProcessState::WAITING);

    // T8: I/O countdown
    runOneTick(state, bus, pm, sched);

    // T9: I/O complete → READY → dispatched
    runOneTick(state, bus, pm, sched);
    EXPECT_EQ(state.runningPID, 1);

    // T10: final tick, process terminates
    runOneTick(state, bus, pm, sched);
    EXPECT_EQ(state.processTable[1].state, ProcessState::TERMINATED);
    EXPECT_EQ(state.processTable[1].remainingBurst, 0u);
}

// ═══════════════════════════════════════════════════════════════
// Test 12: Gantt Policy Snapshot
// ═══════════════════════════════════════════════════════════════

TEST(SchedulerGeneral, GanttPolicySnapshot) {
    SimulationState state;
    EventBus bus;
    ProcessManager pm;
    Scheduler sched;
    sched.setPolicy("FCFS");

    pm.createProcess(state, bus, makeSpec("P1", ProcessType::CPU_BOUND, 5, 3));
    runTicks(3, state, bus, pm, sched);

    // All Gantt entries should record the policy name
    for (const auto& entry : state.ganttLog) {
        EXPECT_EQ(entry.policySnapshot, "FCFS");
    }
}
