/**
 * test_engine_integration.cpp - Phase 6 end-to-end integration tests
 */

#include <gtest/gtest.h>

#include <algorithm>

#include "core/ClockController.h"
#include "core/EventBus.h"
#include "core/SimulationState.h"
#include "modules/memory/MemoryManager.h"
#include "modules/process/ProcessManager.h"
#include "modules/process/ProcessSpec.h"
#include "modules/scheduler/Scheduler.h"
#include "modules/sync/SyncManager.h"

class EngineIntegrationTest : public ::testing::Test {
protected:
    SimulationState state;
    EventBus bus;
    ClockController clock{state, bus};

    std::shared_ptr<ProcessManager> processManager = std::make_shared<ProcessManager>();
    std::shared_ptr<Scheduler> scheduler = std::make_shared<Scheduler>();
    std::shared_ptr<MemoryManager> memoryManager = std::make_shared<MemoryManager>();
    std::shared_ptr<SyncManager> syncManager = std::make_shared<SyncManager>();

    void SetUp() override {
        clock.registerModule(ModuleSlot::PROCESS, processManager);
        clock.registerModule(ModuleSlot::SCHEDULER, scheduler);
        clock.registerModule(ModuleSlot::MEMORY, memoryManager);
        clock.registerModule(ModuleSlot::SYNC, syncManager);
        memoryManager->initializeFrameTable(state, 16);
    }

    void TearDown() override {
        clock.shutdown();
    }

    uint64_t step() {
        uint64_t prev = clock.getCompletedTick();
        EXPECT_TRUE(clock.requestStep());
        EXPECT_TRUE(clock.waitForTickAdvance(prev, 2000));
        return clock.getCompletedTick();
    }

    void createTwoCpuProcesses() {
        ProcessSpec p1;
        p1.name = "p1";
        p1.type = ProcessType::CPU_BOUND;
        p1.priority = 3;
        p1.cpuBurst = 8;
        p1.memoryRequirement = 3;
        processManager->createProcess(state, bus, p1);

        ProcessSpec p2;
        p2.name = "p2";
        p2.type = ProcessType::CPU_BOUND;
        p2.priority = 4;
        p2.cpuBurst = 7;
        p2.memoryRequirement = 3;
        processManager->createProcess(state, bus, p2);
    }
};

TEST_F(EngineIntegrationTest, ModulesRunTogetherWithEventFlow) {
    createTwoCpuProcesses();
    int mutexId = syncManager->createMutex(state, bus, "integration_lock");

    clock.start();

    // Tick 1: admission + first dispatch + first memory touch.
    step();
    EXPECT_EQ(state.currentTick, 1u);
    EXPECT_EQ(state.ganttLog.size(), 1u);
    EXPECT_FALSE(state.pageTables.empty());
    EXPECT_NE(state.runningPID, -1);

    int firstRunning = state.runningPID;
    int otherPid = (firstRunning == 1) ? 2 : 1;
    ASSERT_TRUE(state.processTable.find(otherPid) != state.processTable.end());

    // Tick 2: running process acquires mutex.
    syncManager->requestAcquire(state, firstRunning, mutexId);
    step();
    ASSERT_TRUE(state.mutexTable.find(mutexId) != state.mutexTable.end());
    EXPECT_TRUE(state.mutexTable[mutexId].locked);
    EXPECT_EQ(state.mutexTable[mutexId].ownerPid, firstRunning);

    // Tick 3: other process contends and blocks.
    syncManager->requestAcquire(state, otherPid, mutexId);
    step();
    EXPECT_EQ(state.processTable[otherPid].state, ProcessState::WAITING);
    EXPECT_TRUE(std::find(state.readyQueue.begin(), state.readyQueue.end(), otherPid) == state.readyQueue.end());

    // Tick 4: owner releases and contender is unblocked.
    syncManager->requestRelease(state, firstRunning, mutexId);
    step();
    EXPECT_EQ(state.processTable[otherPid].state, ProcessState::READY);

    // Integration sanity checks after combined execution.
    EXPECT_EQ(state.ganttLog.size(), state.currentTick);
    EXPECT_GT(state.memMetrics.totalPageFaults, 0u);
    EXPECT_GE(state.metrics.totalProcesses, 2u);
    EXPECT_TRUE(syncManager->getStatus() == ModuleStatus::ACTIVE);
}

TEST_F(EngineIntegrationTest, ResetThenRestartWorksInSameControllerInstance) {
    createTwoCpuProcesses();
    clock.start();

    step();
    step();
    EXPECT_GT(state.currentTick, 0u);
    EXPECT_FALSE(state.processTable.empty());

    clock.reset();
    EXPECT_EQ(state.currentTick, 0u);
    EXPECT_TRUE(state.processTable.empty());
    EXPECT_TRUE(state.readyQueue.empty());
    EXPECT_EQ(state.status, SimStatus::IDLE);
    EXPECT_EQ(state.frameTable.size(), 16u);

    // Recreate workload and run again without reconstructing clock.
    createTwoCpuProcesses();
    clock.start();
    step();

    EXPECT_EQ(state.currentTick, 1u);
    EXPECT_EQ(state.ganttLog.size(), 1u);
    EXPECT_NE(state.runningPID, -1);
}
