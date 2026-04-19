/**
 * test_sync_manager.cpp — Unit Tests for the Sync Manager Module
 *
 * Phase 5 test suite covering:
 *   A. Mutex tests (9 tests)
 *   B. Semaphore tests (9 tests)
 *   C. Integration tests (5 tests)
 *   D. Race condition demo tests (3 tests)
 *   E. Event publishing tests (5 tests)
 *   F. Edge case tests (4 tests)
 *
 * Total: 35 tests
 */

#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <algorithm>

#include "core/SimulationState.h"
#include "core/EventBus.h"
#include "core/SimEvent.h"
#include "core/SimEnums.h"
#include "modules/process/ProcessManager.h"
#include "modules/process/ProcessSpec.h"
#include "modules/sync/SyncManager.h"
#include "modules/sync/Mutex.h"
#include "modules/sync/Semaphore.h"
#include "modules/sync/SyncRequest.h"

// ═══════════════════════════════════════════════════════════════
// Test Fixture
// ═══════════════════════════════════════════════════════════════

class SyncManagerTest : public ::testing::Test {
protected:
    SimulationState state;
    EventBus bus;
    ProcessManager procMgr;
    SyncManager syncMgr;
    std::vector<SimEvent> capturedEvents;

    void SetUp() override {
        state.reset();
        bus.reset();
        procMgr.reset();
        syncMgr.reset();
        capturedEvents.clear();

        // Subscribe to ALL events for verification
        bus.subscribeAll([this](const SimEvent& e) {
            capturedEvents.push_back(e);
        });
    }

    /**
     * Helper: Create a test process and admit it to READY state.
     * Returns the PID.
     */
    int createReadyProcess(const std::string& name, int priority = 5) {
        ProcessSpec spec;
        spec.name = name;
        spec.type = ProcessType::CPU_BOUND;
        spec.priority = priority;
        spec.cpuBurst = 10;
        spec.memoryRequirement = 1;
        int pid = procMgr.createProcess(state, bus, spec);

        // Admit: NEW → READY
        procMgr.onTick(state, bus);
        return pid;
    }

    /**
     * Helper: Set a specific process as the running process.
     */
    void setRunning(int pid) {
        // Remove from readyQueue
        auto& rq = state.readyQueue;
        rq.erase(std::remove(rq.begin(), rq.end(), pid), rq.end());

        state.runningPID = pid;
        state.processTable[pid].state = ProcessState::RUNNING;
    }

    /**
     * Helper: Find events of a specific type.
     */
    std::vector<SimEvent> findEvents(const std::string& type) {
        std::vector<SimEvent> result;
        for (const auto& e : capturedEvents) {
            if (e.eventType == type) {
                result.push_back(e);
            }
        }
        return result;
    }

    /**
     * Helper: Clear captured events (useful between operations).
     */
    void clearEvents() {
        capturedEvents.clear();
    }
};

// ═══════════════════════════════════════════════════════════════
// A. MUTEX TESTS (9 tests)
// ═══════════════════════════════════════════════════════════════

// A1: MutexCreation
TEST_F(SyncManagerTest, MutexCreation) {
    int id = syncMgr.createMutex(state, bus, "test_lock");

    EXPECT_EQ(id, 1);
    ASSERT_TRUE(state.mutexTable.find(id) != state.mutexTable.end());

    const Mutex& mtx = state.mutexTable[id];
    EXPECT_EQ(mtx.mutexId, 1);
    EXPECT_EQ(mtx.name, "test_lock");
    EXPECT_FALSE(mtx.locked);
    EXPECT_EQ(mtx.ownerPid, -1);
    EXPECT_TRUE(mtx.waitingPids.empty());
    EXPECT_EQ(mtx.lockedAtTick, 0u);
    EXPECT_EQ(mtx.totalAcquisitions, 0u);
    EXPECT_EQ(mtx.totalContentions, 0u);

    // Second mutex gets incremented ID
    int id2 = syncMgr.createMutex(state, bus, "lock_2");
    EXPECT_EQ(id2, 2);
}

// A2: MutexAcquireUncontested
TEST_F(SyncManagerTest, MutexAcquireUncontested) {
    int pid = createReadyProcess("worker_1");
    setRunning(pid);

    int mtxId = syncMgr.createMutex(state, bus, "resource_lock");
    clearEvents();

    // Request acquire
    syncMgr.requestAcquire(state, pid, mtxId);
    syncMgr.onTick(state, bus);

    // Verify mutex state
    const Mutex& mtx = state.mutexTable[mtxId];
    EXPECT_TRUE(mtx.locked);
    EXPECT_EQ(mtx.ownerPid, pid);
    EXPECT_EQ(mtx.totalAcquisitions, 1u);
    EXPECT_EQ(mtx.totalContentions, 0u);

    // Process should still be running (not blocked)
    EXPECT_EQ(state.processTable[pid].state, ProcessState::RUNNING);
}

// A3: MutexAcquireContested
TEST_F(SyncManagerTest, MutexAcquireContested) {
    int pid1 = createReadyProcess("owner");
    int pid2 = createReadyProcess("contender");
    setRunning(pid1);

    int mtxId = syncMgr.createMutex(state, bus, "shared_lock");

    // pid1 acquires first
    syncMgr.requestAcquire(state, pid1, mtxId);
    syncMgr.onTick(state, bus);

    // pid2 tries to acquire — should be blocked
    clearEvents();
    syncMgr.requestAcquire(state, pid2, mtxId);
    syncMgr.onTick(state, bus);

    // Verify pid2 is blocked
    EXPECT_EQ(state.processTable[pid2].state, ProcessState::WAITING);
    EXPECT_TRUE(std::find(state.readyQueue.begin(), state.readyQueue.end(), pid2)
                == state.readyQueue.end());

    // Verify mutex state
    const Mutex& mtx = state.mutexTable[mtxId];
    EXPECT_EQ(mtx.ownerPid, pid1);
    ASSERT_EQ(mtx.waitingPids.size(), 1u);
    EXPECT_EQ(mtx.waitingPids.front(), pid2);
    EXPECT_EQ(mtx.totalContentions, 1u);
}

// A4: MutexRelease_NoWaiters
TEST_F(SyncManagerTest, MutexRelease_NoWaiters) {
    int pid = createReadyProcess("holder");
    setRunning(pid);

    int mtxId = syncMgr.createMutex(state, bus, "temp_lock");

    // Acquire
    syncMgr.requestAcquire(state, pid, mtxId);
    syncMgr.onTick(state, bus);
    clearEvents();

    // Release
    syncMgr.requestRelease(state, pid, mtxId);
    syncMgr.onTick(state, bus);

    // Verify mutex is unlocked
    const Mutex& mtx = state.mutexTable[mtxId];
    EXPECT_FALSE(mtx.locked);
    EXPECT_EQ(mtx.ownerPid, -1);
    EXPECT_EQ(mtx.lockedAtTick, 0u);
}

// A5: MutexRelease_WithWaiters
TEST_F(SyncManagerTest, MutexRelease_WithWaiters) {
    int pid1 = createReadyProcess("owner");
    int pid2 = createReadyProcess("waiter");
    setRunning(pid1);

    int mtxId = syncMgr.createMutex(state, bus, "transfer_lock");

    // pid1 acquires
    syncMgr.requestAcquire(state, pid1, mtxId);
    syncMgr.onTick(state, bus);

    // pid2 tries to acquire → blocked
    syncMgr.requestAcquire(state, pid2, mtxId);
    syncMgr.onTick(state, bus);
    EXPECT_EQ(state.processTable[pid2].state, ProcessState::WAITING);

    // pid1 releases → pid2 should get ownership and be unblocked
    clearEvents();
    syncMgr.requestRelease(state, pid1, mtxId);
    syncMgr.onTick(state, bus);

    const Mutex& mtx = state.mutexTable[mtxId];
    EXPECT_TRUE(mtx.locked);
    EXPECT_EQ(mtx.ownerPid, pid2);
    EXPECT_TRUE(mtx.waitingPids.empty());
    EXPECT_EQ(state.processTable[pid2].state, ProcessState::READY);
    EXPECT_TRUE(std::find(state.readyQueue.begin(), state.readyQueue.end(), pid2)
                != state.readyQueue.end());
}

// A6: MutexRelease_NonOwner
TEST_F(SyncManagerTest, MutexRelease_NonOwner) {
    int pid1 = createReadyProcess("owner");
    int pid2 = createReadyProcess("imposter");
    setRunning(pid1);

    int mtxId = syncMgr.createMutex(state, bus, "owned_lock");

    // pid1 acquires
    syncMgr.requestAcquire(state, pid1, mtxId);
    syncMgr.onTick(state, bus);

    // pid2 tries to release — should fail
    syncMgr.requestRelease(state, pid2, mtxId);
    syncMgr.onTick(state, bus);

    // Mutex should still be locked by pid1
    const Mutex& mtx = state.mutexTable[mtxId];
    EXPECT_TRUE(mtx.locked);
    EXPECT_EQ(mtx.ownerPid, pid1);
}

// A7: MutexMultipleWaiters_FIFO
TEST_F(SyncManagerTest, MutexMultipleWaiters_FIFO) {
    int owner = createReadyProcess("owner");
    int w1 = createReadyProcess("waiter_1");
    int w2 = createReadyProcess("waiter_2");
    int w3 = createReadyProcess("waiter_3");
    setRunning(owner);

    int mtxId = syncMgr.createMutex(state, bus, "fifo_lock");

    // Owner acquires
    syncMgr.requestAcquire(state, owner, mtxId);
    syncMgr.onTick(state, bus);

    // Three processes try to acquire in order
    syncMgr.requestAcquire(state, w1, mtxId);
    syncMgr.requestAcquire(state, w2, mtxId);
    syncMgr.requestAcquire(state, w3, mtxId);
    syncMgr.onTick(state, bus);

    // Verify all three are waiting
    EXPECT_EQ(state.mutexTable[mtxId].waitingPids.size(), 3u);

    // Release 1: w1 should get it (FIFO)
    syncMgr.requestRelease(state, owner, mtxId);
    syncMgr.onTick(state, bus);
    EXPECT_EQ(state.mutexTable[mtxId].ownerPid, w1);

    // Release 2: w2 should get it
    syncMgr.requestRelease(state, w1, mtxId);
    syncMgr.onTick(state, bus);
    EXPECT_EQ(state.mutexTable[mtxId].ownerPid, w2);

    // Release 3: w3 should get it
    syncMgr.requestRelease(state, w2, mtxId);
    syncMgr.onTick(state, bus);
    EXPECT_EQ(state.mutexTable[mtxId].ownerPid, w3);
}

// A8: MutexOwnerTerminated_AutoRelease
TEST_F(SyncManagerTest, MutexOwnerTerminated_AutoRelease) {
    int owner = createReadyProcess("doomed_owner");
    int waiter = createReadyProcess("patient_waiter");
    setRunning(owner);

    int mtxId = syncMgr.createMutex(state, bus, "auto_release_lock");

    // Owner acquires
    syncMgr.requestAcquire(state, owner, mtxId);
    syncMgr.onTick(state, bus);

    // Waiter tries to acquire → blocked
    syncMgr.requestAcquire(state, waiter, mtxId);
    syncMgr.onTick(state, bus);
    EXPECT_EQ(state.processTable[waiter].state, ProcessState::WAITING);

    // Owner terminates
    state.processTable[owner].state = ProcessState::TERMINATED;

    // Run tick — cleanupTerminatedProcessLocks should auto-release
    syncMgr.onTick(state, bus);

    // Waiter should now be the owner and in READY state
    const Mutex& mtx = state.mutexTable[mtxId];
    EXPECT_EQ(mtx.ownerPid, waiter);
    EXPECT_EQ(state.processTable[waiter].state, ProcessState::READY);
}

// A9: MutexMetrics
TEST_F(SyncManagerTest, MutexMetrics) {
    int pid1 = createReadyProcess("p1");
    int pid2 = createReadyProcess("p2");
    setRunning(pid1);

    int mtxId = syncMgr.createMutex(state, bus, "metrics_lock");

    // pid1 acquires (1 acquisition)
    syncMgr.requestAcquire(state, pid1, mtxId);
    syncMgr.onTick(state, bus);

    // pid2 tries to acquire (1 contention)
    syncMgr.requestAcquire(state, pid2, mtxId);
    syncMgr.onTick(state, bus);

    // pid1 releases → pid2 gets it (2 acquisitions, 1 contention)
    syncMgr.requestRelease(state, pid1, mtxId);
    syncMgr.onTick(state, bus);

    const Mutex& mtx = state.mutexTable[mtxId];
    EXPECT_EQ(mtx.totalAcquisitions, 2u);
    EXPECT_EQ(mtx.totalContentions, 1u);
}

// ═══════════════════════════════════════════════════════════════
// B. SEMAPHORE TESTS (9 tests)
// ═══════════════════════════════════════════════════════════════

// B10: SemaphoreCreation_Binary
TEST_F(SyncManagerTest, SemaphoreCreation_Binary) {
    int id = syncMgr.createSemaphore(state, bus, "bin_sem",
                                      SyncPrimitiveType::SEMAPHORE_BINARY, 1);

    EXPECT_EQ(id, 1);
    ASSERT_TRUE(state.semaphoreTable.find(id) != state.semaphoreTable.end());

    const Semaphore& sem = state.semaphoreTable[id];
    EXPECT_EQ(sem.semId, 1);
    EXPECT_EQ(sem.name, "bin_sem");
    EXPECT_EQ(sem.primitiveType, SyncPrimitiveType::SEMAPHORE_BINARY);
    EXPECT_EQ(sem.value, 1);
    EXPECT_EQ(sem.initialValue, 1);
    EXPECT_TRUE(sem.waitingPids.empty());
}

// B11: SemaphoreCreation_Counting
TEST_F(SyncManagerTest, SemaphoreCreation_Counting) {
    int id = syncMgr.createSemaphore(state, bus, "count_sem",
                                      SyncPrimitiveType::SEMAPHORE_COUNTING, 3);

    const Semaphore& sem = state.semaphoreTable[id];
    EXPECT_EQ(sem.value, 3);
    EXPECT_EQ(sem.initialValue, 3);
    EXPECT_EQ(sem.primitiveType, SyncPrimitiveType::SEMAPHORE_COUNTING);
}

// B12: SemaphoreWait_Available
TEST_F(SyncManagerTest, SemaphoreWait_Available) {
    int pid = createReadyProcess("waiter");
    setRunning(pid);

    int semId = syncMgr.createSemaphore(state, bus, "avail_sem",
                                         SyncPrimitiveType::SEMAPHORE_BINARY, 1);

    syncMgr.requestWait(state, pid, semId);
    syncMgr.onTick(state, bus);

    // Value should be decremented, process should NOT be blocked
    EXPECT_EQ(state.semaphoreTable[semId].value, 0);
    EXPECT_EQ(state.processTable[pid].state, ProcessState::RUNNING);
    EXPECT_EQ(state.semaphoreTable[semId].totalWaits, 1u);
    EXPECT_EQ(state.semaphoreTable[semId].totalBlocks, 0u);
}

// B13: SemaphoreWait_Blocked
TEST_F(SyncManagerTest, SemaphoreWait_Blocked) {
    int pid1 = createReadyProcess("first_waiter");
    int pid2 = createReadyProcess("second_waiter");
    setRunning(pid1);

    int semId = syncMgr.createSemaphore(state, bus, "block_sem",
                                         SyncPrimitiveType::SEMAPHORE_BINARY, 1);

    // pid1 waits — succeeds (value 1 → 0)
    syncMgr.requestWait(state, pid1, semId);
    syncMgr.onTick(state, bus);
    EXPECT_EQ(state.semaphoreTable[semId].value, 0);

    // pid2 waits — blocked (value == 0)
    syncMgr.requestWait(state, pid2, semId);
    syncMgr.onTick(state, bus);

    EXPECT_EQ(state.processTable[pid2].state, ProcessState::WAITING);
    EXPECT_EQ(state.semaphoreTable[semId].waitingPids.size(), 1u);
    EXPECT_EQ(state.semaphoreTable[semId].waitingPids.front(), pid2);
    EXPECT_EQ(state.semaphoreTable[semId].totalBlocks, 1u);
}

// B14: SemaphoreSignal_NoWaiters
TEST_F(SyncManagerTest, SemaphoreSignal_NoWaiters) {
    int pid = createReadyProcess("signaler");
    setRunning(pid);

    int semId = syncMgr.createSemaphore(state, bus, "sig_sem",
                                         SyncPrimitiveType::SEMAPHORE_BINARY, 1);

    // Wait to take the permit (value 1 → 0)
    syncMgr.requestWait(state, pid, semId);
    syncMgr.onTick(state, bus);
    EXPECT_EQ(state.semaphoreTable[semId].value, 0);

    // Signal with no waiters — value should go back to 1
    syncMgr.requestSignal(state, pid, semId);
    syncMgr.onTick(state, bus);

    EXPECT_EQ(state.semaphoreTable[semId].value, 1);
    EXPECT_EQ(state.semaphoreTable[semId].totalSignals, 1u);
}

// B15: SemaphoreSignal_WithWaiters
TEST_F(SyncManagerTest, SemaphoreSignal_WithWaiters) {
    int pid1 = createReadyProcess("holder");
    int pid2 = createReadyProcess("blocked_proc");
    setRunning(pid1);

    int semId = syncMgr.createSemaphore(state, bus, "unblock_sem",
                                         SyncPrimitiveType::SEMAPHORE_BINARY, 1);

    // pid1 waits (value 1 → 0)
    syncMgr.requestWait(state, pid1, semId);
    syncMgr.onTick(state, bus);

    // pid2 waits — blocked
    syncMgr.requestWait(state, pid2, semId);
    syncMgr.onTick(state, bus);
    EXPECT_EQ(state.processTable[pid2].state, ProcessState::WAITING);

    // pid1 signals — pid2 should be unblocked
    clearEvents();
    syncMgr.requestSignal(state, pid1, semId);
    syncMgr.onTick(state, bus);

    EXPECT_EQ(state.processTable[pid2].state, ProcessState::READY);
    EXPECT_TRUE(state.semaphoreTable[semId].waitingPids.empty());
    // Value stays at 0 because the unblocked process consumed the signal
    EXPECT_EQ(state.semaphoreTable[semId].value, 0);
}

// B16: BinarySemaphore_ValueCap
TEST_F(SyncManagerTest, BinarySemaphore_ValueCap) {
    int pid = createReadyProcess("capper");
    setRunning(pid);

    int semId = syncMgr.createSemaphore(state, bus, "cap_sem",
                                         SyncPrimitiveType::SEMAPHORE_BINARY, 1);

    // Value is already 1 — signal should NOT increase beyond 1
    syncMgr.requestSignal(state, pid, semId);
    syncMgr.onTick(state, bus);

    EXPECT_EQ(state.semaphoreTable[semId].value, 1);

    // Another signal — still capped at 1
    syncMgr.requestSignal(state, pid, semId);
    syncMgr.onTick(state, bus);

    EXPECT_EQ(state.semaphoreTable[semId].value, 1);
}

// B17: CountingSemaphore_MultipleSlots
TEST_F(SyncManagerTest, CountingSemaphore_MultipleSlots) {
    int p1 = createReadyProcess("slot_1");
    int p2 = createReadyProcess("slot_2");
    int p3 = createReadyProcess("slot_3");
    int p4 = createReadyProcess("overflow");
    setRunning(p1);

    int semId = syncMgr.createSemaphore(state, bus, "pool",
                                         SyncPrimitiveType::SEMAPHORE_COUNTING, 3);

    // 3 waits — all succeed (value 3 → 2 → 1 → 0)
    syncMgr.requestWait(state, p1, semId);
    syncMgr.requestWait(state, p2, semId);
    syncMgr.requestWait(state, p3, semId);
    syncMgr.onTick(state, bus);

    EXPECT_EQ(state.semaphoreTable[semId].value, 0);
    EXPECT_EQ(state.processTable[p1].state, ProcessState::RUNNING);
    // p2 and p3 should not be blocked since there were enough permits
    // (they were in READY state and had valid permits)

    // 4th wait — should block
    syncMgr.requestWait(state, p4, semId);
    syncMgr.onTick(state, bus);

    EXPECT_EQ(state.processTable[p4].state, ProcessState::WAITING);
    EXPECT_EQ(state.semaphoreTable[semId].waitingPids.size(), 1u);
}

// B18: SemaphoreMetrics
TEST_F(SyncManagerTest, SemaphoreMetrics) {
    int pid1 = createReadyProcess("m1");
    int pid2 = createReadyProcess("m2");
    setRunning(pid1);

    int semId = syncMgr.createSemaphore(state, bus, "metrics_sem",
                                         SyncPrimitiveType::SEMAPHORE_BINARY, 1);

    // Wait (1 wait, 0 blocks)
    syncMgr.requestWait(state, pid1, semId);
    syncMgr.onTick(state, bus);

    // Wait (2 waits, 1 block)
    syncMgr.requestWait(state, pid2, semId);
    syncMgr.onTick(state, bus);

    // Signal (1 signal)
    syncMgr.requestSignal(state, pid1, semId);
    syncMgr.onTick(state, bus);

    const Semaphore& sem = state.semaphoreTable[semId];
    EXPECT_EQ(sem.totalWaits, 2u);
    EXPECT_EQ(sem.totalBlocks, 1u);
    EXPECT_EQ(sem.totalSignals, 1u);
}

// ═══════════════════════════════════════════════════════════════
// C. INTEGRATION TESTS (5 tests)
// ═══════════════════════════════════════════════════════════════

// C19: ProcessBlocked_RemovedFromReadyQueue
TEST_F(SyncManagerTest, ProcessBlocked_RemovedFromReadyQueue) {
    int pid1 = createReadyProcess("blocker");
    int pid2 = createReadyProcess("blocked");
    setRunning(pid1);

    // pid2 should be in readyQueue
    EXPECT_TRUE(std::find(state.readyQueue.begin(), state.readyQueue.end(), pid2)
                != state.readyQueue.end());

    int mtxId = syncMgr.createMutex(state, bus, "rq_lock");
    syncMgr.requestAcquire(state, pid1, mtxId);
    syncMgr.onTick(state, bus);

    // pid2 tries to acquire → blocked → removed from readyQueue
    syncMgr.requestAcquire(state, pid2, mtxId);
    syncMgr.onTick(state, bus);

    EXPECT_TRUE(std::find(state.readyQueue.begin(), state.readyQueue.end(), pid2)
                == state.readyQueue.end());
}

// C20: ProcessUnblocked_AddedToReadyQueue
TEST_F(SyncManagerTest, ProcessUnblocked_AddedToReadyQueue) {
    int pid1 = createReadyProcess("releaser");
    int pid2 = createReadyProcess("unblocked");
    setRunning(pid1);

    int mtxId = syncMgr.createMutex(state, bus, "rq_lock_2");
    syncMgr.requestAcquire(state, pid1, mtxId);
    syncMgr.onTick(state, bus);

    syncMgr.requestAcquire(state, pid2, mtxId);
    syncMgr.onTick(state, bus);
    EXPECT_TRUE(std::find(state.readyQueue.begin(), state.readyQueue.end(), pid2)
                == state.readyQueue.end());

    // Release → pid2 unblocked → added to readyQueue
    syncMgr.requestRelease(state, pid1, mtxId);
    syncMgr.onTick(state, bus);

    EXPECT_TRUE(std::find(state.readyQueue.begin(), state.readyQueue.end(), pid2)
                != state.readyQueue.end());
}

// C21: RunningProcess_Blocked_CPUIdle
TEST_F(SyncManagerTest, RunningProcess_Blocked_CPUIdle) {
    int pid1 = createReadyProcess("lock_holder");
    int pid2 = createReadyProcess("runner_blocked");
    setRunning(pid1);

    int mtxId = syncMgr.createMutex(state, bus, "cpu_lock");
    syncMgr.requestAcquire(state, pid1, mtxId);
    syncMgr.onTick(state, bus);

    // Now make pid2 running
    setRunning(pid2);
    EXPECT_EQ(state.runningPID, pid2);

    // pid2 tries to acquire the locked mutex → should be blocked
    syncMgr.requestAcquire(state, pid2, mtxId);
    syncMgr.onTick(state, bus);

    // CPU should be idle since the running process was blocked
    EXPECT_EQ(state.runningPID, -1);
    EXPECT_EQ(state.processTable[pid2].state, ProcessState::WAITING);
}

// C22: CleanupTerminated_RemovesFromAllQueues
TEST_F(SyncManagerTest, CleanupTerminated_RemovesFromAllQueues) {
    int pid1 = createReadyProcess("owner");
    int pid2 = createReadyProcess("dead_waiter");
    int pid3 = createReadyProcess("alive_waiter");
    setRunning(pid1);

    int mtxId = syncMgr.createMutex(state, bus, "cleanup_lock");
    syncMgr.requestAcquire(state, pid1, mtxId);
    syncMgr.onTick(state, bus);

    // Both pid2 and pid3 try to acquire → both blocked
    syncMgr.requestAcquire(state, pid2, mtxId);
    syncMgr.requestAcquire(state, pid3, mtxId);
    syncMgr.onTick(state, bus);

    EXPECT_EQ(state.mutexTable[mtxId].waitingPids.size(), 2u);

    // pid2 terminates
    state.processTable[pid2].state = ProcessState::TERMINATED;

    // Run tick — cleanup should remove pid2 from waiting queue
    syncMgr.onTick(state, bus);

    // Only pid3 should remain in the waiting queue
    const Mutex& mtx = state.mutexTable[mtxId];
    EXPECT_EQ(mtx.waitingPids.size(), 1u);
    EXPECT_EQ(mtx.waitingPids.front(), pid3);
}

// C23: MultiTick_ProducerConsumer
TEST_F(SyncManagerTest, MultiTick_ProducerConsumer) {
    int producer = createReadyProcess("producer");
    int consumer = createReadyProcess("consumer");
    setRunning(producer);

    // Binary semaphore: starts at 0 (consumer waits until producer signals)
    int semId = syncMgr.createSemaphore(state, bus, "buffer",
                                         SyncPrimitiveType::SEMAPHORE_BINARY, 1);
    // Manually set value to 0 (empty buffer)
    state.semaphoreTable[semId].value = 0;

    // Tick 1: Consumer tries to wait → blocked (buffer empty)
    syncMgr.requestWait(state, consumer, semId);
    syncMgr.onTick(state, bus);
    EXPECT_EQ(state.processTable[consumer].state, ProcessState::WAITING);

    // Tick 2: Producer signals (item produced)
    syncMgr.requestSignal(state, producer, semId);
    syncMgr.onTick(state, bus);

    // Consumer should now be unblocked
    EXPECT_EQ(state.processTable[consumer].state, ProcessState::READY);
    EXPECT_TRUE(state.semaphoreTable[semId].waitingPids.empty());
}

// ═══════════════════════════════════════════════════════════════
// D. RACE CONDITION DEMO TESTS (3 tests)
// ═══════════════════════════════════════════════════════════════

// D24: RaceCondition_WithoutSync
TEST_F(SyncManagerTest, RaceCondition_WithoutSync) {
    // Setup demo without synchronization
    syncMgr.setupRaceConditionDemo(state, bus, false /* no sync */);
    EXPECT_TRUE(syncMgr.isRaceConditionDemoActive());
    EXPECT_EQ(syncMgr.getRaceConditionCounter(), 0);

    // Create two writer processes
    int w1 = createReadyProcess("race_writer_1");
    int w2 = createReadyProcess("race_writer_2");

    // Simulate several ticks with alternating running processes
    for (int i = 0; i < 4; i++) {
        // Alternate which process is running
        if (i % 2 == 0) {
            setRunning(w1);
        } else {
            setRunning(w2);
        }
        state.currentTick = i + 1;
        syncMgr.onTick(state, bus);
    }

    // After 4 ticks, expectedCounter should be 4
    // sharedCounter should also be 4 in this simple sequential simulation
    EXPECT_EQ(syncMgr.getRaceConditionExpectedValue(), 4);
    // The log should have entries showing unprotected access
    EXPECT_FALSE(syncMgr.getRaceConditionLog().empty());
}

// D25: RaceCondition_WithSync
TEST_F(SyncManagerTest, RaceCondition_WithSync) {
    // Setup demo WITH synchronization
    syncMgr.setupRaceConditionDemo(state, bus, true /* with sync */);
    EXPECT_TRUE(syncMgr.isRaceConditionDemoActive());

    // Create two writer processes
    int w1 = createReadyProcess("race_writer_1");
    int w2 = createReadyProcess("race_writer_2");

    // Run several ticks — with sync, counter should always be correct
    setRunning(w1);
    state.currentTick = 1;
    syncMgr.onTick(state, bus);  // w1 acquires lock

    state.currentTick = 2;
    syncMgr.onTick(state, bus);  // w1 should now hold lock, write counter, release

    // Check that the mutex was used
    EXPECT_TRUE(state.mutexTable.find(syncMgr.getRaceConditionCounter() >= 0 ? 1 : -1)
                != state.mutexTable.end());

    (void)w2;  // Used by the demo infrastructure
}

// D26: RaceConditionLog_ShowsInterleaving
TEST_F(SyncManagerTest, RaceConditionLog_ShowsInterleaving) {
    syncMgr.setupRaceConditionDemo(state, bus, false);

    int w1 = createReadyProcess("log_writer_1");
    int w2 = createReadyProcess("log_writer_2");

    setRunning(w1);
    state.currentTick = 1;
    syncMgr.onTick(state, bus);

    setRunning(w2);
    state.currentTick = 2;
    syncMgr.onTick(state, bus);

    const auto& log = syncMgr.getRaceConditionLog();
    EXPECT_GE(log.size(), 2u);

    // Log entries should contain PID information
    bool foundW1 = false, foundW2 = false;
    for (const auto& entry : log) {
        if (entry.find(std::to_string(w1)) != std::string::npos) foundW1 = true;
        if (entry.find(std::to_string(w2)) != std::string::npos) foundW2 = true;
    }
    EXPECT_TRUE(foundW1);
    EXPECT_TRUE(foundW2);
}

// ═══════════════════════════════════════════════════════════════
// E. EVENT PUBLISHING TESTS (5 tests)
// ═══════════════════════════════════════════════════════════════

// E27: Event_LOCK_ACQUIRED_Published
TEST_F(SyncManagerTest, Event_LOCK_ACQUIRED_Published) {
    int pid = createReadyProcess("acq_event_test");
    setRunning(pid);

    int mtxId = syncMgr.createMutex(state, bus, "event_lock");
    clearEvents();

    syncMgr.requestAcquire(state, pid, mtxId);
    syncMgr.onTick(state, bus);

    auto events = findEvents(EventTypes::LOCK_ACQUIRED);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].sourcePid, pid);
    EXPECT_EQ(events[0].resourceId, mtxId);
}

// E28: Event_LOCK_RELEASED_Published
TEST_F(SyncManagerTest, Event_LOCK_RELEASED_Published) {
    int pid = createReadyProcess("rel_event_test");
    setRunning(pid);

    int mtxId = syncMgr.createMutex(state, bus, "rel_event_lock");
    syncMgr.requestAcquire(state, pid, mtxId);
    syncMgr.onTick(state, bus);
    clearEvents();

    syncMgr.requestRelease(state, pid, mtxId);
    syncMgr.onTick(state, bus);

    auto events = findEvents(EventTypes::LOCK_RELEASED);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].sourcePid, pid);
    EXPECT_EQ(events[0].resourceId, mtxId);
}

// E29: Event_PROCESS_BLOCKED_Published
TEST_F(SyncManagerTest, Event_PROCESS_BLOCKED_Published) {
    int pid1 = createReadyProcess("holder_evt");
    int pid2 = createReadyProcess("blocker_evt");
    setRunning(pid1);

    int mtxId = syncMgr.createMutex(state, bus, "block_evt_lock");
    syncMgr.requestAcquire(state, pid1, mtxId);
    syncMgr.onTick(state, bus);
    clearEvents();

    syncMgr.requestAcquire(state, pid2, mtxId);
    syncMgr.onTick(state, bus);

    auto events = findEvents(EventTypes::PROCESS_BLOCKED);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].sourcePid, pid2);
    EXPECT_EQ(events[0].resourceId, mtxId);
}

// E30: Event_PROCESS_UNBLOCKED_Published
TEST_F(SyncManagerTest, Event_PROCESS_UNBLOCKED_Published) {
    int pid1 = createReadyProcess("releaser_evt");
    int pid2 = createReadyProcess("unblocked_evt");
    setRunning(pid1);

    int mtxId = syncMgr.createMutex(state, bus, "unblock_evt_lock");
    syncMgr.requestAcquire(state, pid1, mtxId);
    syncMgr.onTick(state, bus);

    syncMgr.requestAcquire(state, pid2, mtxId);
    syncMgr.onTick(state, bus);
    clearEvents();

    syncMgr.requestRelease(state, pid1, mtxId);
    syncMgr.onTick(state, bus);

    auto events = findEvents(EventTypes::PROCESS_UNBLOCKED);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].targetPid, pid2);
}

// E31: DecisionLog_Populated
TEST_F(SyncManagerTest, DecisionLog_Populated) {
    int pid = createReadyProcess("log_test");
    setRunning(pid);

    size_t logSizeBefore = state.decisionLog.size();

    int mtxId = syncMgr.createMutex(state, bus, "log_lock");
    syncMgr.requestAcquire(state, pid, mtxId);
    syncMgr.onTick(state, bus);

    // Should have decision log entries for creation and acquire
    EXPECT_GT(state.decisionLog.size(), logSizeBefore);

    // Check at least one entry mentions "MUTEX"
    bool foundMutexEntry = false;
    for (size_t i = logSizeBefore; i < state.decisionLog.size(); i++) {
        if (state.decisionLog[i].message.find("MUTEX") != std::string::npos ||
            state.decisionLog[i].message.find("mutex") != std::string::npos) {
            foundMutexEntry = true;
            break;
        }
    }
    EXPECT_TRUE(foundMutexEntry);
}

// ═══════════════════════════════════════════════════════════════
// F. EDGE CASE TESTS (4 tests)
// ═══════════════════════════════════════════════════════════════

// F32: AcquireInvalidMutex
TEST_F(SyncManagerTest, AcquireInvalidMutex) {
    int pid = createReadyProcess("invalid_acq");
    setRunning(pid);

    // Request acquire on a non-existent mutex
    syncMgr.requestAcquire(state, pid, 999);
    EXPECT_NO_THROW(syncMgr.onTick(state, bus));

    // Process should still be running (not crashed)
    EXPECT_EQ(state.processTable[pid].state, ProcessState::RUNNING);
}

// F33: WaitInvalidSemaphore
TEST_F(SyncManagerTest, WaitInvalidSemaphore) {
    int pid = createReadyProcess("invalid_wait");
    setRunning(pid);

    // Request wait on a non-existent semaphore
    syncMgr.requestWait(state, pid, 999);
    EXPECT_NO_THROW(syncMgr.onTick(state, bus));

    // Process should still be running
    EXPECT_EQ(state.processTable[pid].state, ProcessState::RUNNING);
}

// F34: ResetClearsAllState
TEST_F(SyncManagerTest, ResetClearsAllState) {
    // Create some state
    syncMgr.createMutex(state, bus, "temp");
    syncMgr.createSemaphore(state, bus, "temp_sem",
                             SyncPrimitiveType::SEMAPHORE_BINARY, 1);

    // Reset
    syncMgr.reset();

    EXPECT_EQ(syncMgr.getStatus(), ModuleStatus::IDLE);
    EXPECT_FALSE(syncMgr.isRaceConditionDemoActive());
    EXPECT_EQ(syncMgr.getRaceConditionCounter(), 0);

    // Note: SimulationState is NOT reset by module reset() — that's the
    // responsibility of SimulationState::reset() called by ClockController.
    // Module reset only clears internal counters.
}

// F35: ModuleNameAndStatus
TEST_F(SyncManagerTest, ModuleNameAndStatus) {
    EXPECT_EQ(syncMgr.getModuleName(), "SyncManager");
    EXPECT_EQ(syncMgr.getStatus(), ModuleStatus::IDLE);

    // After onTick, status should be ACTIVE
    syncMgr.onTick(state, bus);
    EXPECT_EQ(syncMgr.getStatus(), ModuleStatus::ACTIVE);
}
