/**
 * test_clock_controller.cpp - Phase 6 Clock Controller tests
 */

#include <gtest/gtest.h>

#include <chrono>
#include <thread>
#include <vector>

#include "core/ClockController.h"
#include "core/EventBus.h"
#include "core/SimEvent.h"
#include "core/SimulationState.h"
#include "modules/memory/MemoryManager.h"
#include "modules/process/ProcessManager.h"
#include "modules/process/ProcessSpec.h"
#include "modules/scheduler/Scheduler.h"
#include "modules/sync/SyncManager.h"

using namespace std::chrono_literals;

class ClockControllerTest : public ::testing::Test {
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

    void createBasicWorkload() {
        ProcessSpec a;
        a.name = "a";
        a.type = ProcessType::CPU_BOUND;
        a.priority = 3;
        a.cpuBurst = 8;
        a.memoryRequirement = 3;
        processManager->createProcess(state, bus, a);

        ProcessSpec b;
        b.name = "b";
        b.type = ProcessType::CPU_BOUND;
        b.priority = 5;
        b.cpuBurst = 6;
        b.memoryRequirement = 2;
        processManager->createProcess(state, bus, b);
    }
};

TEST_F(ClockControllerTest, StepModeAdvancesExactlyOneTickPerRequest) {
    createBasicWorkload();
    clock.start();

    uint64_t t0 = clock.getCompletedTick();
    ASSERT_TRUE(clock.requestStep());
    ASSERT_TRUE(clock.waitForTickAdvance(t0, 2000));
    uint64_t t1 = clock.getCompletedTick();
    EXPECT_EQ(t1, t0 + 1);

    ASSERT_TRUE(clock.requestStep());
    ASSERT_TRUE(clock.waitForTickAdvance(t1, 2000));
    uint64_t t2 = clock.getCompletedTick();
    EXPECT_EQ(t2, t1 + 1);
}

TEST_F(ClockControllerTest, AutoModeAdvancesWithoutManualStep) {
    createBasicWorkload();
    clock.start();
    clock.setMode(SimMode::AUTO);
    clock.setAutoSpeedMs(30);

    uint64_t before = clock.getCompletedTick();
    std::this_thread::sleep_for(220ms);
    uint64_t after = clock.getCompletedTick();

    EXPECT_GT(after, before);
}

TEST_F(ClockControllerTest, PauseStopsTickAdvancement) {
    createBasicWorkload();
    clock.start();
    clock.setMode(SimMode::AUTO);
    clock.setAutoSpeedMs(20);

    std::this_thread::sleep_for(180ms);
    clock.pause();
    uint64_t pausedTick = clock.getCompletedTick();

    std::this_thread::sleep_for(200ms);
    EXPECT_EQ(clock.getCompletedTick(), pausedTick);
}

TEST_F(ClockControllerTest, ResetClearsStateAndPreservesSubscribers) {
    createBasicWorkload();

    int tickEventsSeen = 0;
    bus.subscribe(EventTypes::TICK_ADVANCED, [&](const SimEvent&) {
        tickEventsSeen++;
    });

    clock.start();
    uint64_t t0 = clock.getCompletedTick();
    ASSERT_TRUE(clock.requestStep());
    ASSERT_TRUE(clock.waitForTickAdvance(t0, 2000));
    EXPECT_EQ(tickEventsSeen, 1);

    clock.reset();
    EXPECT_EQ(state.currentTick, 0u);
    EXPECT_EQ(state.status, SimStatus::IDLE);
    EXPECT_TRUE(state.processTable.empty());
    EXPECT_TRUE(state.readyQueue.empty());
    EXPECT_EQ(state.frameTable.size(), 16u);

    // Subscriber should still receive TICK_ADVANCED after reset.
    createBasicWorkload();
    clock.start();
    uint64_t t1 = clock.getCompletedTick();
    ASSERT_TRUE(clock.requestStep());
    ASSERT_TRUE(clock.waitForTickAdvance(t1, 2000));
    EXPECT_EQ(tickEventsSeen, 2);
}

TEST_F(ClockControllerTest, ShutdownExitsWithoutDeadlock) {
    createBasicWorkload();
    clock.start();
    clock.setMode(SimMode::AUTO);
    clock.setAutoSpeedMs(25);

    std::this_thread::sleep_for(120ms);
    clock.shutdown();

    // Idempotency check.
    clock.shutdown();
    SUCCEED();
}
