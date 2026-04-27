/**
 * test_memory_manager.cpp — Memory Manager Module Unit Tests
 *
 * Phase 4 test suite for the Memory Manager.
 *
 * Test groups:
 *   1. Policy Unit Tests (FIFO, LRU isolated)
 *   2. Page Table Initialization
 *   3. Page Fault Handling
 *   4. FIFO Textbook Reference String (3 frames, 15 faults expected)
 *   5. LRU Textbook Reference String (3 frames, 12 faults expected)
 *   6. Policy Hot-Swap
 *   7. Frame Table Management
 *   8. Metrics
 */

#include <gtest/gtest.h>

#include "core/SimulationState.h"
#include "core/EventBus.h"
#include "core/SimEnums.h"
#include "core/SimEvent.h"
#include "modules/process/ProcessManager.h"
#include "modules/process/ProcessSpec.h"
#include "modules/memory/MemoryManager.h"
#include "modules/memory/IReplacementPolicy.h"
#include "modules/memory/policies/FIFOPolicy.h"
#include "modules/memory/policies/LRUPolicy.h"
#include "modules/memory/FrameTableEntry.h"
#include "modules/memory/PageTable.h"
#include "modules/memory/PageTableEntry.h"
#include "modules/memory/MemoryMetrics.h"

#include <algorithm>

// ═══════════════════════════════════════════════════════════════
// Helper fixture
// ═══════════════════════════════════════════════════════════════

class MemoryManagerTest : public ::testing::Test {
protected:
    SimulationState state;
    EventBus bus;
    ProcessManager procMgr;
    MemoryManager memMgr;

    void SetUp() override {
        // State and bus are default-constructed (Phase 1 verified)
    }

    /**
     * Helper: create a process and tick through Process Manager
     * so it moves from NEW → READY.
     */
    int createAndAdmitProcess(const std::string& name, uint32_t memReq,
                              uint32_t cpuBurst = 20, int priority = 5) {
        ProcessSpec spec;
        spec.name = name;
        spec.type = ProcessType::CPU_BOUND;
        spec.priority = priority;
        spec.cpuBurst = cpuBurst;
        spec.ioBurstDuration = 0;
        spec.memoryRequirement = memReq;
        int pid = procMgr.createProcess(state, bus, spec);

        // Tick process manager to admit NEW → READY
        procMgr.onTick(state, bus);
        return pid;
    }

    /**
     * Helper: make a process the running process.
     */
    void makeRunning(int pid) {
        auto it = state.processTable.find(pid);
        if (it != state.processTable.end()) {
            it->second.state = ProcessState::RUNNING;
            state.runningPID = pid;
            // Remove from ready queue
            auto rqIt = std::find(state.readyQueue.begin(), state.readyQueue.end(), pid);
            if (rqIt != state.readyQueue.end()) {
                state.readyQueue.erase(rqIt);
            }
        }
    }
};


// ═══════════════════════════════════════════════════════════════
// Group 1: Policy Unit Tests (Isolated)
// ═══════════════════════════════════════════════════════════════

TEST(PolicyTests, FIFO_SelectsOldestLoaded) {
    FIFOPolicy policy;
    std::vector<FrameTableEntry> frames(4);
    for (int i = 0; i < 4; i++) {
        frames[i].frameNumber = i;
        frames[i].occupied = true;
        frames[i].ownerPid = 1;
    }
    frames[0].loadTick = 5;
    frames[1].loadTick = 1;   // Oldest — should be selected
    frames[2].loadTick = 3;
    frames[3].loadTick = 7;

    int victim = policy.selectVictimFrame(frames);
    EXPECT_EQ(victim, 1);
}

TEST(PolicyTests, FIFO_PolicyName) {
    FIFOPolicy policy;
    EXPECT_EQ(policy.policyName(), "FIFO");
}

TEST(PolicyTests, LRU_SelectsLeastRecentlyUsed) {
    LRUPolicy policy;
    std::vector<FrameTableEntry> frames(4);
    for (int i = 0; i < 4; i++) {
        frames[i].frameNumber = i;
        frames[i].occupied = true;
        frames[i].ownerPid = 1;
    }
    frames[0].lastAccessTick = 5;
    frames[1].lastAccessTick = 2;   // Least recently used — should be selected
    frames[2].lastAccessTick = 7;
    frames[3].lastAccessTick = 3;

    int victim = policy.selectVictimFrame(frames);
    EXPECT_EQ(victim, 1);
}

TEST(PolicyTests, LRU_PolicyName) {
    LRUPolicy policy;
    EXPECT_EQ(policy.policyName(), "LRU");
}

TEST(PolicyTests, FIFO_AllSameLoadTick_SelectsFirstFrame) {
    FIFOPolicy policy;
    std::vector<FrameTableEntry> frames(4);
    for (int i = 0; i < 4; i++) {
        frames[i].frameNumber = i;
        frames[i].occupied = true;
        frames[i].loadTick = 10;
    }

    int victim = policy.selectVictimFrame(frames);
    EXPECT_EQ(victim, 0);  // First frame with tied loadTick
}

TEST(PolicyTests, LRU_AllSameAccessTick_SelectsFirstFrame) {
    LRUPolicy policy;
    std::vector<FrameTableEntry> frames(4);
    for (int i = 0; i < 4; i++) {
        frames[i].frameNumber = i;
        frames[i].occupied = true;
        frames[i].lastAccessTick = 10;
    }

    int victim = policy.selectVictimFrame(frames);
    EXPECT_EQ(victim, 0);  // First frame with tied access
}

TEST(PolicyTests, FIFO_SkipsUnoccupiedFrames) {
    FIFOPolicy policy;
    std::vector<FrameTableEntry> frames(4);
    for (int i = 0; i < 4; i++) {
        frames[i].frameNumber = i;
        frames[i].occupied = false;
    }
    frames[2].occupied = true;
    frames[2].loadTick = 5;

    int victim = policy.selectVictimFrame(frames);
    EXPECT_EQ(victim, 2);  // Only occupied frame
}

TEST(PolicyTests, NoOccupiedFrames_ReturnsNegative) {
    FIFOPolicy policy;
    std::vector<FrameTableEntry> frames(4);
    for (int i = 0; i < 4; i++) {
        frames[i].frameNumber = i;
        frames[i].occupied = false;
    }

    int victim = policy.selectVictimFrame(frames);
    EXPECT_EQ(victim, -1);
}


// ═══════════════════════════════════════════════════════════════
// Group 2: Page Table Initialization
// ═══════════════════════════════════════════════════════════════

TEST_F(MemoryManagerTest, InitPageTable_OnAdmission) {
    memMgr.initializeFrameTable(state, 8);
    int pid = createAndAdmitProcess("test_proc", 4);

    // Page table should not exist yet (memory manager hasn't ticked)
    EXPECT_EQ(state.pageTables.find(pid), state.pageTables.end());

    // Tick memory manager — should create page table
    memMgr.onTick(state, bus);

    auto ptIt = state.pageTables.find(pid);
    ASSERT_NE(ptIt, state.pageTables.end());
    EXPECT_EQ(ptIt->second.ownerPid, pid);
    EXPECT_EQ(ptIt->second.entries.size(), 4u);

    // All entries should be invalid initially
    for (const auto& pte : ptIt->second.entries) {
        EXPECT_FALSE(pte.valid);
        EXPECT_EQ(pte.frameNumber, -1);
    }
}

TEST_F(MemoryManagerTest, InitPageTable_MultipleProcesses) {
    memMgr.initializeFrameTable(state, 16);

    int pid1 = createAndAdmitProcess("proc_a", 3);
    int pid2 = createAndAdmitProcess("proc_b", 5);
    int pid3 = createAndAdmitProcess("proc_c", 2);

    memMgr.onTick(state, bus);

    EXPECT_NE(state.pageTables.find(pid1), state.pageTables.end());
    EXPECT_NE(state.pageTables.find(pid2), state.pageTables.end());
    EXPECT_NE(state.pageTables.find(pid3), state.pageTables.end());

    EXPECT_EQ(state.pageTables[pid1].entries.size(), 3u);
    EXPECT_EQ(state.pageTables[pid2].entries.size(), 5u);
    EXPECT_EQ(state.pageTables[pid3].entries.size(), 2u);
}

TEST_F(MemoryManagerTest, InitPageTable_DoesNotDuplicate) {
    memMgr.initializeFrameTable(state, 8);
    int pid = createAndAdmitProcess("test_proc", 4);

    memMgr.onTick(state, bus);  // Creates page table
    ASSERT_NE(state.pageTables.find(pid), state.pageTables.end());

    size_t entryCount = state.pageTables[pid].entries.size();
    memMgr.onTick(state, bus);  // Should not recreate
    EXPECT_EQ(state.pageTables[pid].entries.size(), entryCount);
}


// ═══════════════════════════════════════════════════════════════
// Group 3: Page Fault Handling
// ═══════════════════════════════════════════════════════════════

TEST_F(MemoryManagerTest, FirstAccess_CausesPageFault) {
    memMgr.initializeFrameTable(state, 8);
    int pid = createAndAdmitProcess("test_proc", 4);
    makeRunning(pid);

    // Set explicit access sequence: access VPN 0
    memMgr.setAccessSequence({0});

    // First tick: creates page table
    memMgr.onTick(state, bus);
    state.currentTick++;

    // Second tick: accesses VPN 0 — page fault
    memMgr.setAccessSequence({0});
    memMgr.onTick(state, bus);

    EXPECT_EQ(state.processTable[pid].pageFaultCount, 1u);
    EXPECT_EQ(state.memMetrics.totalPageFaults, 1u);

    // VPN 0 should now be valid
    auto ptIt = state.pageTables.find(pid);
    ASSERT_NE(ptIt, state.pageTables.end());
    EXPECT_TRUE(ptIt->second.entries[0].valid);
    EXPECT_GE(ptIt->second.entries[0].frameNumber, 0);
}

TEST_F(MemoryManagerTest, SecondAccess_SameVPN_NoFault) {
    memMgr.initializeFrameTable(state, 8);
    int pid = createAndAdmitProcess("test_proc", 4);
    makeRunning(pid);

    // Tick 1: init page table + access VPN 0 (fault)
    memMgr.setAccessSequence({0});
    memMgr.onTick(state, bus);
    state.currentTick++;
    uint32_t faultsAfterFirst = state.memMetrics.totalPageFaults;

    // Tick 2: access VPN 0 again (hit)
    memMgr.setAccessSequence({0});
    memMgr.onTick(state, bus);

    EXPECT_EQ(state.memMetrics.totalPageFaults, faultsAfterFirst);
}

TEST_F(MemoryManagerTest, AllFramesFull_EvictionRequired) {
    // Only 2 frames, process needs 4 pages
    memMgr.initializeFrameTable(state, 4);  // min is 4
    int pid = createAndAdmitProcess("test_proc", 6);
    makeRunning(pid);

    // Access VPN 0, 1, 2, 3 to fill all 4 frames
    memMgr.setAccessSequence({0, 1, 2, 3});

    for (int i = 0; i < 4; i++) {
        memMgr.onTick(state, bus);
        state.currentTick++;
    }

    EXPECT_EQ(state.memMetrics.totalPageFaults, 4u);
    EXPECT_EQ(state.memMetrics.occupiedFrames, 4u);
    EXPECT_EQ(state.memMetrics.totalReplacements, 0u);

    // Now access VPN 4 — must evict
    memMgr.setAccessSequence({4});
    memMgr.onTick(state, bus);

    EXPECT_EQ(state.memMetrics.totalPageFaults, 5u);
    EXPECT_EQ(state.memMetrics.totalReplacements, 1u);
}


// ═══════════════════════════════════════════════════════════════
// Group 4: FIFO Textbook Reference String
//
// Reference string: 7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1
// Frames: 3
// Expected page faults: 15 (Belady's classic FIFO example)
// ═══════════════════════════════════════════════════════════════

TEST_F(MemoryManagerTest, FIFO_TextbookReferenceString_15Faults) {
    // Configure: 3 physical frames
    memMgr.initializeFrameTable(state, 4);  // min 4 by DD, but we need exactly 3
    // Override: manually create 3 frames
    state.frameTable.clear();
    for (int i = 0; i < 3; i++) {
        FrameTableEntry entry;
        entry.frameNumber = i;
        entry.occupied = false;
        entry.ownerPid = -1;
        state.frameTable.push_back(entry);
    }
    state.memMetrics.totalFrames = 3;

    // Create process with 8 virtual pages (pages 0-7 in reference string)
    int pid = createAndAdmitProcess("fifo_test", 8, 30);
    makeRunning(pid);

    // Set FIFO policy (default)
    memMgr.setPolicy("FIFO");
    EXPECT_EQ(memMgr.getActivePolicyName(), "FIFO");

    // Classic reference string
    std::vector<uint32_t> refString = {7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1};
    memMgr.setAccessSequence(refString);

    // Run one tick per reference — page table gets created on first tick
    // and first access happens on the same tick
    for (size_t i = 0; i < refString.size(); i++) {
        memMgr.onTick(state, bus);
        state.currentTick++;
    }

    // FIFO with 3 frames and this reference string = 15 page faults
    EXPECT_EQ(state.memMetrics.totalPageFaults, 15u)
        << "FIFO reference string should produce exactly 15 page faults with 3 frames";
}


// ═══════════════════════════════════════════════════════════════
// Group 5: LRU Textbook Reference String
//
// Reference string: 7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1
// Frames: 3
// Expected page faults: 12 (textbook LRU)
// ═══════════════════════════════════════════════════════════════

TEST_F(MemoryManagerTest, LRU_TextbookReferenceString_12Faults) {
    // Configure: 3 physical frames
    state.frameTable.clear();
    for (int i = 0; i < 3; i++) {
        FrameTableEntry entry;
        entry.frameNumber = i;
        entry.occupied = false;
        entry.ownerPid = -1;
        state.frameTable.push_back(entry);
    }
    state.memMetrics.totalFrames = 3;

    // Create process with 8 virtual pages (pages 0-7 in reference string)
    int pid = createAndAdmitProcess("lru_test", 8, 30);
    makeRunning(pid);

    // Set LRU policy
    memMgr.setPolicy("LRU");
    EXPECT_EQ(memMgr.getActivePolicyName(), "LRU");

    // Classic reference string
    std::vector<uint32_t> refString = {7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1};
    memMgr.setAccessSequence(refString);

    for (size_t i = 0; i < refString.size(); i++) {
        memMgr.onTick(state, bus);
        state.currentTick++;
    }

    // LRU with 3 frames and this reference string = 12 page faults
    EXPECT_EQ(state.memMetrics.totalPageFaults, 12u)
        << "LRU reference string should produce exactly 12 page faults with 3 frames";
}


// ═══════════════════════════════════════════════════════════════
// Group 6: Policy Hot-Swap
// ═══════════════════════════════════════════════════════════════

TEST_F(MemoryManagerTest, DefaultPolicy_IsFIFO) {
    EXPECT_EQ(memMgr.getActivePolicyName(), "FIFO");
}

TEST_F(MemoryManagerTest, SwapPolicy_FIFOtoLRU) {
    memMgr.setPolicy("LRU");
    EXPECT_EQ(memMgr.getActivePolicyName(), "LRU");
}

TEST_F(MemoryManagerTest, SwapPolicy_LRUtoFIFO) {
    memMgr.setPolicy("LRU");
    memMgr.setPolicy("FIFO");
    EXPECT_EQ(memMgr.getActivePolicyName(), "FIFO");
}

TEST_F(MemoryManagerTest, SwapPolicy_InvalidName_KeepsCurrent) {
    memMgr.setPolicy("FIFO");
    memMgr.setPolicy("INVALID_POLICY");
    EXPECT_EQ(memMgr.getActivePolicyName(), "FIFO");
}

TEST_F(MemoryManagerTest, SwapPolicy_MidSimulation) {
    memMgr.initializeFrameTable(state, 4);
    // Manually set up 3 frames for this test
    state.frameTable.clear();
    for (int i = 0; i < 3; i++) {
        FrameTableEntry entry;
        entry.frameNumber = i;
        entry.occupied = false;
        entry.ownerPid = -1;
        state.frameTable.push_back(entry);
    }
    state.memMetrics.totalFrames = 3;

    int pid = createAndAdmitProcess("swap_test", 5, 20);
    makeRunning(pid);

    // Fill frames with FIFO: access VPN 0, 1, 2
    memMgr.setAccessSequence({0, 1, 2});
    for (int i = 0; i < 3; i++) {
        memMgr.onTick(state, bus);
        state.currentTick++;
    }
    EXPECT_EQ(state.memMetrics.totalPageFaults, 3u);

    // Swap to LRU
    memMgr.setPolicy("LRU");
    EXPECT_EQ(memMgr.getActivePolicyName(), "LRU");

    // Access VPN 0 (hit — updates lastAccess so 0 is most recent)
    memMgr.setAccessSequence({0});
    memMgr.onTick(state, bus);
    state.currentTick++;

    // Access VPN 3 — should evict LRU (VPN 1, least recently used)
    memMgr.setAccessSequence({3});
    memMgr.onTick(state, bus);
    state.currentTick++;

    EXPECT_EQ(state.memMetrics.totalPageFaults, 4u);

    // VPN 1 should now be invalid (evicted by LRU)
    auto ptIt = state.pageTables.find(pid);
    ASSERT_NE(ptIt, state.pageTables.end());
    EXPECT_FALSE(ptIt->second.entries[1].valid);
}


// ═══════════════════════════════════════════════════════════════
// Group 7: Frame Table Management
// ═══════════════════════════════════════════════════════════════

TEST_F(MemoryManagerTest, InitializeFrameTable_CorrectSize) {
    memMgr.initializeFrameTable(state, 8);
    EXPECT_EQ(state.frameTable.size(), 8u);
    EXPECT_EQ(state.memMetrics.totalFrames, 8u);

    for (int i = 0; i < 8; i++) {
        EXPECT_EQ(state.frameTable[i].frameNumber, i);
        EXPECT_FALSE(state.frameTable[i].occupied);
        EXPECT_EQ(state.frameTable[i].ownerPid, -1);
    }
}

TEST_F(MemoryManagerTest, InitializeFrameTable_MinimumEnforced) {
    memMgr.initializeFrameTable(state, 2);  // Below minimum of 4
    EXPECT_EQ(state.frameTable.size(), 4u);  // Should be clamped to 4
    EXPECT_EQ(state.memMetrics.totalFrames, 4u);
}

TEST_F(MemoryManagerTest, InitializeFrameTable_CustomSize) {
    memMgr.initializeFrameTable(state, 32);
    EXPECT_EQ(state.frameTable.size(), 32u);
    EXPECT_EQ(state.memMetrics.totalFrames, 32u);
}

TEST_F(MemoryManagerTest, OccupiedFrameCount_AfterLoading) {
    memMgr.initializeFrameTable(state, 8);
    int pid = createAndAdmitProcess("test_proc", 4);
    makeRunning(pid);

    memMgr.setAccessSequence({0, 1, 2});
    for (int i = 0; i < 3; i++) {
        memMgr.onTick(state, bus);
        state.currentTick++;
    }

    EXPECT_EQ(state.memMetrics.occupiedFrames, 3u);
}

TEST_F(MemoryManagerTest, CleanupTerminatedProcess_FreesFrames) {
    memMgr.initializeFrameTable(state, 8);
    int pid = createAndAdmitProcess("test_proc", 3);
    makeRunning(pid);

    // Load 3 pages
    memMgr.setAccessSequence({0, 1, 2});
    for (int i = 0; i < 3; i++) {
        memMgr.onTick(state, bus);
        state.currentTick++;
    }
    EXPECT_EQ(state.memMetrics.occupiedFrames, 3u);

    // Terminate the process
    state.processTable[pid].state = ProcessState::TERMINATED;
    state.runningPID = -1;

    memMgr.onTick(state, bus);

    // Frames should be freed
    EXPECT_EQ(state.memMetrics.occupiedFrames, 0u);
    // Page table should be removed
    EXPECT_EQ(state.pageTables.find(pid), state.pageTables.end());
}


// ═══════════════════════════════════════════════════════════════
// Group 8: Metrics
// ═══════════════════════════════════════════════════════════════

TEST_F(MemoryManagerTest, PageFaultRate_Calculation) {
    memMgr.initializeFrameTable(state, 8);
    int pid = createAndAdmitProcess("test_proc", 4);
    makeRunning(pid);

    // Access VPN 0, 1, 0, 1, 0 — first two are faults, rest are hits
    memMgr.setAccessSequence({0, 1, 0, 1, 0});
    for (int i = 0; i < 5; i++) {
        memMgr.onTick(state, bus);
        state.currentTick++;
    }

    // 2 faults out of 5 accesses = 40%
    EXPECT_EQ(state.memMetrics.totalPageFaults, 2u);
    EXPECT_NEAR(state.memMetrics.pageFaultRate, 40.0f, 1.0f);
}

TEST_F(MemoryManagerTest, TotalReplacements_CountsEvictions) {
    // Set up 4 frames (minimum)
    state.frameTable.clear();
    for (int i = 0; i < 4; i++) {
        FrameTableEntry entry;
        entry.frameNumber = i;
        entry.occupied = false;
        entry.ownerPid = -1;
        state.frameTable.push_back(entry);
    }
    state.memMetrics.totalFrames = 4;

    int pid = createAndAdmitProcess("test_proc", 8);
    makeRunning(pid);

    // Fill all 4 frames: VPN 0, 1, 2, 3
    memMgr.setAccessSequence({0, 1, 2, 3});
    for (int i = 0; i < 4; i++) {
        memMgr.onTick(state, bus);
        state.currentTick++;
    }
    EXPECT_EQ(state.memMetrics.totalReplacements, 0u);

    // Now 3 more accesses requiring eviction: VPN 4, 5, 6
    memMgr.setAccessSequence({4, 5, 6});
    for (int i = 0; i < 3; i++) {
        memMgr.onTick(state, bus);
        state.currentTick++;
    }
    EXPECT_EQ(state.memMetrics.totalReplacements, 3u);
}

TEST_F(MemoryManagerTest, ActivePolicy_UpdatedInMetrics) {
    memMgr.initializeFrameTable(state, 8);
    int pid = createAndAdmitProcess("test_proc", 2);
    (void)pid;

    // Default should be FIFO
    memMgr.onTick(state, bus);
    EXPECT_EQ(state.memMetrics.activePolicy, "FIFO");

    // Swap to LRU
    memMgr.setPolicy("LRU");
    state.currentTick++;
    memMgr.onTick(state, bus);
    EXPECT_EQ(state.memMetrics.activePolicy, "LRU");
}


// ═══════════════════════════════════════════════════════════════
// Group 9: Module Interface
// ═══════════════════════════════════════════════════════════════

TEST_F(MemoryManagerTest, ModuleName) {
    EXPECT_EQ(memMgr.getModuleName(), "MemoryManager");
}

TEST_F(MemoryManagerTest, InitialStatus_IsIdle) {
    EXPECT_EQ(memMgr.getStatus(), ModuleStatus::IDLE);
}

TEST_F(MemoryManagerTest, Status_ActiveAfterTick) {
    memMgr.initializeFrameTable(state, 8);
    memMgr.onTick(state, bus);
    EXPECT_EQ(memMgr.getStatus(), ModuleStatus::ACTIVE);
}

TEST_F(MemoryManagerTest, Reset_ClearsState) {
    memMgr.initializeFrameTable(state, 8);
    int pid = createAndAdmitProcess("test_proc", 4);
    makeRunning(pid);
    memMgr.setAccessSequence({0});
    memMgr.onTick(state, bus);

    memMgr.reset();

    EXPECT_EQ(memMgr.getStatus(), ModuleStatus::IDLE);
    EXPECT_EQ(memMgr.getActivePolicyName(), "FIFO");  // Reset to default
}


// ═══════════════════════════════════════════════════════════════
// Group 10: Edge Cases
// ═══════════════════════════════════════════════════════════════

TEST_F(MemoryManagerTest, NoRunningProcess_NoAccess) {
    memMgr.initializeFrameTable(state, 8);
    int pid = createAndAdmitProcess("test_proc", 4);
    (void)pid;  // Not used — point is that no process is RUNNING
    // Don't make it running — leave it in READY

    memMgr.onTick(state, bus);

    // No faults should occur since no process is running
    EXPECT_EQ(state.memMetrics.totalPageFaults, 0u);
}

TEST_F(MemoryManagerTest, DefaultAccess_UsesLocalityOfReference) {
    memMgr.initializeFrameTable(state, 8);
    int pid = createAndAdmitProcess("test_proc", 3);
    makeRunning(pid);

    // Tick 1: first default access faults on VPN 0.
    memMgr.onTick(state, bus);
    EXPECT_EQ(state.memMetrics.totalPageFaults, 1u);
    state.currentTick++;

    // Tick 2: immediate re-access should hit same local page.
    memMgr.onTick(state, bus);
    EXPECT_EQ(state.memMetrics.totalPageFaults, 1u);
    state.currentTick++;

    // Continue a few more ticks: locality still limits distinct pages.
    for (int i = 0; i < 4; i++) {
        memMgr.onTick(state, bus);
        state.currentTick++;
    }

    EXPECT_LE(state.memMetrics.totalPageFaults, 3u);
    EXPECT_LE(state.memMetrics.occupiedFrames, 3u);
}

TEST_F(MemoryManagerTest, DefaultLocalityPattern_LRUFaultsRemainComparableToFIFO) {
    auto runDefaultPattern = [&](const std::string& policyName) {
        SimulationState localState;
        EventBus localBus;
        ProcessManager localProcMgr;
        MemoryManager localMemMgr;

        localMemMgr.setPolicy(policyName);
        localMemMgr.initializeFrameTable(localState, 4);

        ProcessSpec spec;
        spec.name = policyName + "_locality_test";
        spec.type = ProcessType::CPU_BOUND;
        spec.priority = 5;
        spec.cpuBurst = 80;
        spec.ioBurstDuration = 0;
        spec.memoryRequirement = 8;

        int pid = localProcMgr.createProcess(localState, localBus, spec);
        localProcMgr.onTick(localState, localBus);
        localState.processTable[pid].state = ProcessState::RUNNING;
        localState.runningPID = pid;
        localState.readyQueue.clear();

        for (int i = 0; i < 80; i++) {
            localMemMgr.onTick(localState, localBus);
            localState.currentTick++;
        }

        return localState.memMetrics.totalPageFaults;
    };

    uint32_t fifoFaults = runDefaultPattern("FIFO");
    uint32_t lruFaults = runDefaultPattern("LRU");

    uint32_t diff = (lruFaults > fifoFaults) ? (lruFaults - fifoFaults) : (fifoFaults - lruFaults);

    EXPECT_GT(fifoFaults, 0u);
    EXPECT_GT(lruFaults, 0u);
    EXPECT_LE(diff, 2u)
        << "Default locality pattern should keep FIFO and LRU fault counts comparable";
}

TEST_F(MemoryManagerTest, MultipleProcesses_SeparatePageTables) {
    memMgr.initializeFrameTable(state, 16);

    int pid1 = createAndAdmitProcess("proc_a", 3);
    int pid2 = createAndAdmitProcess("proc_b", 2);

    // Make pid1 running and access its pages
    makeRunning(pid1);
    memMgr.setAccessSequence({0, 1});
    for (int i = 0; i < 2; i++) {
        memMgr.onTick(state, bus);
        state.currentTick++;
    }

    // Switch to pid2
    state.processTable[pid1].state = ProcessState::READY;
    state.readyQueue.push_back(pid1);
    makeRunning(pid2);

    memMgr.setAccessSequence({0, 1});
    for (int i = 0; i < 2; i++) {
        memMgr.onTick(state, bus);
        state.currentTick++;
    }

    // pid1 has 2 pages loaded, pid2 has 2 pages loaded
    EXPECT_EQ(state.memMetrics.occupiedFrames, 4u);
    EXPECT_EQ(state.memMetrics.totalPageFaults, 4u);

    // Both page tables should exist with correct sizes
    EXPECT_EQ(state.pageTables[pid1].entries.size(), 3u);
    EXPECT_EQ(state.pageTables[pid2].entries.size(), 2u);
}
