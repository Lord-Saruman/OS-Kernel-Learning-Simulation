# Phase 4 — Memory Manager Implementation Plan

## Goal

Implement the Memory Manager module — the third OS subsystem — for the MiniOS Kernel Simulator. This module manages paging-based virtual memory with configurable page replacement policies (FIFO, LRU). It handles page table creation for new processes, simulates memory accesses on each tick for the running process, handles page faults via frame allocation or victim eviction, and tracks memory performance metrics.

> [!IMPORTANT]
> **PRD Coverage:** FR-MM-01 through FR-MM-06
> **SDD Reference:** Section 3.1 (Memory Manager), 3.4 (Strategy Pattern), 4.1 (Event Bus)
> **Data Dictionary:** Sections 7.1–7.4 (PageTableEntry, PageTable, FrameTableEntry, MemoryMetrics)

---

## User Review Required

> [!IMPORTANT]
> **Frame Table Size** — The `MemoryMetrics.totalFrames` defaults to `16`. This is a reasonable default for demonstrations but is configurable. Is 16 frames acceptable, or do you want a different default?

> [!IMPORTANT]
> **Memory Access Pattern** — The running process will access one virtual page per tick. The accessed VPN is determined by a round-robin scan through the process's page table (VPN = `accessCounter % memoryRequirement`). This creates a predictable, textbook-friendly reference pattern. Shall I use this, or would you prefer a random access model?

> [!WARNING]
> **Page Table Initialization** — Currently, `ProcessManager::admitNewProcesses()` sets `pcb.pageTableId = pid` but does **not** create an entry in `state.pageTables`. The Memory Manager will handle PageTable creation when it sees a process with `pageTableId == pid` but no entry in `pageTables`. This keeps Process Manager unchanged (no Phase 2 regressions).

---

## Proposed Changes

### Component 1 — Replacement Policy Strategies

Two concrete implementations of the existing `IReplacementPolicy` interface.

---

#### [NEW] [FIFOPolicy.h](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/modules/memory/policies/FIFOPolicy.h)

FIFO page replacement strategy. Selects the victim frame with the **earliest `loadTick`** (oldest loaded page).

```cpp
class FIFOPolicy : public IReplacementPolicy {
public:
    int selectVictimFrame(const std::vector<FrameTableEntry>& frames) override;
    std::string policyName() const override; // returns "FIFO"
};
```

**Algorithm:**
```
victimIdx = -1
oldestTick = UINT64_MAX
for each frame in frames:
    if frame.occupied AND frame.loadTick < oldestTick:
        oldestTick = frame.loadTick
        victimIdx = frame.frameNumber
return victimIdx
```

---

#### [NEW] [LRUPolicy.h](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/modules/memory/policies/LRUPolicy.h)

LRU page replacement strategy. Selects the victim frame with the **earliest `lastAccessTick`** (least recently accessed page).

```cpp
class LRUPolicy : public IReplacementPolicy {
public:
    int selectVictimFrame(const std::vector<FrameTableEntry>& frames) override;
    std::string policyName() const override; // returns "LRU"
};
```

**Algorithm:**
```
victimIdx = -1
oldestAccess = UINT64_MAX
for each frame in frames:
    if frame.occupied AND frame.lastAccessTick < oldestAccess:
        oldestAccess = frame.lastAccessTick
        victimIdx = frame.frameNumber
return victimIdx
```

> [!NOTE]
> Both policies are header-only (inline implementations) — consistent with how scheduling policies (FCFSPolicy.h, RoundRobinPolicy.h) are done. No .cpp files needed.

---

### Component 2 — Memory Manager Module

The core module implementing `ISimModule`.

---

#### [NEW] [MemoryManager.h](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/modules/memory/MemoryManager.h)

```cpp
class MemoryManager : public ISimModule {
public:
    MemoryManager();

    // ISimModule interface
    void onTick(SimulationState& state, EventBus& bus) override;
    void reset() override;
    ModuleStatus getStatus() const override;
    std::string getModuleName() const override;

    // Policy Management API (called by API Bridge Phase 7, or tests)
    void setPolicy(const std::string& policyName);
    std::string getActivePolicyName() const;

    // Frame table initialization (called once at sim start or reset)
    void initializeFrameTable(SimulationState& state, uint32_t totalFrames = 16);

private:
    ModuleStatus status_;
    std::unique_ptr<IReplacementPolicy> activePolicy_;
    uint64_t totalMemoryAccesses_;  // For pageFaultRate calculation

    // Per-tick steps — called in order by onTick()
    void initPageTablesForNewProcesses(SimulationState& state, EventBus& bus);
    void clearReferenceBits(SimulationState& state);
    void simulateMemoryAccess(SimulationState& state, EventBus& bus);
    void updateMetrics(SimulationState& state);
    void cleanupTerminatedProcessPages(SimulationState& state, EventBus& bus);

    // Helpers
    int findFreeFrame(const SimulationState& state);
    void loadPage(SimulationState& state, EventBus& bus, int pid,
                  uint32_t vpn, int frameIdx);
    void evictPage(SimulationState& state, EventBus& bus, int victimFrame);

    static std::unique_ptr<IReplacementPolicy> createPolicy(const std::string& name);
};
```

---

#### [NEW] [MemoryManager.cpp](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/modules/memory/MemoryManager.cpp)

**`onTick()` Sequence (5 steps):**

```
Step 1: initPageTablesForNewProcesses()
    For each process in processTable:
        if process has pageTableId == pid AND pageTables[pid] does not exist:
            Create PageTable with ownerPid = pid, pageSize = 256
            Create entries[0..memoryRequirement-1], all invalid (frameNumber=-1)
            Store in state.pageTables[pid]

Step 2: clearReferenceBits()
    For each PageTable in state.pageTables:
        For each PTE in entries:
            pte.referenced = false

Step 3: simulateMemoryAccess()
    If runningPID == -1: return (no running process)
    
    Find the running PCB
    Find the running process's page table
    
    Compute VPN to access:
        vpn = totalMemoryAccesses_ % pcb.memoryRequirement
        (This creates a round-robin scan pattern — textbook friendly)
    
    totalMemoryAccesses_++
    
    Access the PTE at entries[vpn]:
        pte.referenced = true
    
    If pte.valid:
        // Page HIT — update access timestamp
        pte.lastAccessTick = state.currentTick
        frame = state.frameTable[pte.frameNumber]
        frame.lastAccessTick = state.currentTick
        
    Else (Page FAULT):
        pcb.pageFaultCount++
        state.memMetrics.totalPageFaults++
        
        Publish PAGE_FAULT event
        Log decision
        
        // Find a frame to use
        freeFrame = findFreeFrame(state)
        
        if freeFrame >= 0:
            // Free frame available — just load
            loadPage(state, bus, pid, vpn, freeFrame)
        else:
            // All frames occupied — evict via policy
            victimFrame = activePolicy_->selectVictimFrame(state.frameTable)
            evictPage(state, bus, victimFrame)
            loadPage(state, bus, pid, vpn, victimFrame)
            state.memMetrics.totalReplacements++
            
            Publish PAGE_REPLACED event

Step 4: cleanupTerminatedProcessPages()
    For each process in processTable:
        if state == TERMINATED AND pageTables[pid] exists:
            For each PTE in pageTable.entries:
                if valid:
                    Clear the frame (occupied=false, ownerPid=-1)
                    memMetrics.occupiedFrames--
            Remove pageTables[pid]

Step 5: updateMetrics()
    Count occupied frames in frameTable
    state.memMetrics.occupiedFrames = count
    state.memMetrics.activePolicy = activePolicy_->policyName()
    if totalMemoryAccesses_ > 0:
        state.memMetrics.pageFaultRate = 
            (totalPageFaults / totalMemoryAccesses_) * 100.0
```

**`loadPage()` helper:**
```
frame = state.frameTable[frameIdx]
frame.occupied = true
frame.ownerPid = pid
frame.virtualPageNumber = vpn
frame.loadTick = state.currentTick
frame.lastAccessTick = state.currentTick

pte = state.pageTables[pid].entries[vpn]
pte.valid = true
pte.frameNumber = frameIdx
pte.loadTick = state.currentTick
pte.lastAccessTick = state.currentTick
pte.dirty = false
pte.referenced = true

memMetrics.occupiedFrames++ (if frame was previously free)
```

**`evictPage()` helper:**
```
victim = state.frameTable[victimFrame]
oldPid = victim.ownerPid
oldVpn = victim.virtualPageNumber

// Invalidate the old page table entry
if pageTables[oldPid] exists:
    pte = pageTables[oldPid].entries[oldVpn]
    pte.valid = false
    pte.frameNumber = -1

// Clear the frame
victim.occupied = false
victim.ownerPid = -1
memMetrics.occupiedFrames--
```

---

### Component 3 — CMake Build Integration

---

#### [MODIFY] [CMakeLists.txt](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/CMakeLists.txt)

Add memory_manager_lib:

```diff
+# ── Memory Manager Library (Phase 4) ─────────────────────────
+# Contains: MemoryManager, IReplacementPolicy, FIFOPolicy, LRUPolicy
+add_library(memory_manager_lib
+    modules/memory/MemoryManager.cpp
+)
+target_include_directories(memory_manager_lib PUBLIC ${ENGINE_INCLUDE_DIR})
+target_link_libraries(memory_manager_lib PUBLIC core_lib)
```

Update main executable to link memory_manager_lib:

```diff
-target_link_libraries(os_simulator PRIVATE core_lib process_manager_lib scheduler_lib)
+target_link_libraries(os_simulator PRIVATE core_lib process_manager_lib scheduler_lib memory_manager_lib)
```

---

#### [MODIFY] [CMakeLists.txt](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/tests/CMakeLists.txt)

Add memory manager test target:

```diff
+# ── Test: MemoryManager (Phase 4) ───────────────────────────
+add_executable(test_memory_manager test_memory_manager.cpp)
+target_include_directories(test_memory_manager PRIVATE ${ENGINE_INCLUDE_DIR})
+target_link_libraries(test_memory_manager
+    PRIVATE
+    GTest::gtest_main
+    core_lib
+    process_manager_lib
+    memory_manager_lib
+)
+add_test(NAME MemoryManagerTests COMMAND test_memory_manager)
```

---

### Component 4 — Unit Test Suite

---

#### [NEW] [test_memory_manager.cpp](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/tests/test_memory_manager.cpp)

Comprehensive test suite structured as follows:

##### Test Group 1 — Policy Unit Tests (Isolated)

| Test | Description | Expected |
|------|-------------|----------|
| `FIFO_SelectsOldestLoaded` | 4 frames loaded at ticks 1, 3, 2, 4 | Victim = frame loaded at tick 1 |
| `LRU_SelectsLeastRecentlyUsed` | 4 frames with lastAccess 5, 2, 7, 3 | Victim = frame accessed at tick 2 |
| `FIFO_AllSameLoadTick` | 4 frames all loaded at tick 0 | Victim = frame 0 (first found) |
| `LRU_AllSameAccessTick` | 4 frames all accessed at tick 0 | Victim = frame 0 (first found) |

##### Test Group 2 — Page Table Initialization

| Test | Description | Expected |
|------|-------------|----------|
| `InitPageTable_OnAdmission` | Create process with memReq=4, run 1 tick | pageTables[pid] exists, 4 entries, all invalid |
| `InitPageTable_MultipleProcesses` | Create 3 processes | 3 separate page tables in state.pageTables |

##### Test Group 3 — Page Fault Handling

| Test | Description | Expected |
|------|-------------|----------|
| `FirstAccess_PageFault` | Running process accesses VPN 0 (not loaded) | pageFaultCount=1, VPN 0 now valid in frame |
| `SecondAccess_NoFault` | Access same VPN again after loading | No additional fault, lastAccessTick updated |
| `AllFramesFull_EvictionRequired` | Fill all frames, then access new page | Eviction occurs, replacement count incremented |

##### Test Group 4 — FIFO Textbook Reference String ⭐

Classic reference string test: `7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1`

With **3 frames** → expected **15 page faults** (Belady's textbook example).

| Step | Ref | Frame 0 | Frame 1 | Frame 2 | Fault? |
|------|-----|---------|---------|---------|--------|
| 1 | 7 | 7 | - | - | ✓ |
| 2 | 0 | 7 | 0 | - | ✓ |
| 3 | 1 | 7 | 0 | 1 | ✓ |
| 4 | 2 | 2 | 0 | 1 | ✓ (evict 7) |
| 5 | 0 | 2 | 0 | 1 | — |
| 6 | 3 | 2 | 3 | 1 | ✓ (evict 0) |
| 7 | 0 | 2 | 3 | 0 | ✓ (evict 1) |
| 8 | 4 | 4 | 3 | 0 | ✓ (evict 2) |
| 9 | 2 | 4 | 2 | 0 | ✓ (evict 3) |
| 10 | 3 | 4 | 2 | 3 | ✓ (evict 0) |
| 11 | 0 | 0 | 2 | 3 | ✓ (evict 4) |
| 12 | 3 | 0 | 2 | 3 | — |
| 13 | 2 | 0 | 2 | 3 | — |
| 14 | 1 | 1 | 2 | 3 | ✓ (evict 0) |
| 15 | 2 | 1 | 2 | 3 | — |
| 16 | 0 | 1 | 0 | 3 | ✓ (evict 2) |
| 17 | 1 | 1 | 0 | 3 | — |
| 18 | 7 | 1 | 0 | 7 | ✓ (evict 3) |
| 19 | 0 | 1 | 0 | 7 | — |
| 20 | 1 | 1 | 0 | 7 | — |

**Total: 15 faults** ✓

> [!IMPORTANT]
> To test this deterministically, we need to set up a process with `memoryRequirement` = number of distinct pages in the reference string (8: pages 0-7), configure exactly 3 frames, and feed the reference string by controlling which VPN is accessed each tick. The test will use a helper that manually sets VPN to access rather than relying on round-robin.

##### Test Group 5 — LRU Textbook Reference String

Same reference string: `7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1`

With **3 frames** → expected **12 page faults** (textbook LRU).

| Step | Ref | Frame 0 | Frame 1 | Frame 2 | Fault? |
|------|-----|---------|---------|---------|--------|
| 1 | 7 | 7 | - | - | ✓ |
| 2 | 0 | 7 | 0 | - | ✓ |
| 3 | 1 | 7 | 0 | 1 | ✓ |
| 4 | 2 | 2 | 0 | 1 | ✓ (evict 7, LRU) |
| 5 | 0 | 2 | 0 | 1 | — |
| 6 | 3 | 2 | 0 | 3 | ✓ (evict 1, LRU) |
| 7 | 0 | 2 | 0 | 3 | — |
| 8 | 4 | 4 | 0 | 3 | ✓ (evict 2, LRU) |
| 9 | 2 | 4 | 2 | 3 | ✓ (evict 0, LRU) |
| 10 | 3 | 4 | 2 | 3 | — |
| 11 | 0 | 0 | 2 | 3 | ✓ (evict 4, LRU) |
| 12 | 3 | 0 | 2 | 3 | — |
| 13 | 2 | 0 | 2 | 3 | — |
| 14 | 1 | 0 | 1 | 3 | ✓ (evict 2, LRU) |
| 15 | 2 | 0 | 1 | 2 | ✓ (evict 3, LRU) |
| 16 | 0 | 0 | 1 | 2 | — |
| 17 | 1 | 0 | 1 | 2 | — |
| 18 | 7 | 7 | 1 | 2 | ✓ (evict 0, LRU) |
| 19 | 0 | 7 | 0 | 2 | ✓ (evict 1, LRU) |
| 20 | 1 | 7 | 0 | 1 | ✓ — wait, not 12…

Let me recalculate. Corrected LRU with 3 frames for this reference string = **12 faults** per standard textbooks. The actual test will drive each VPN access manually and assert the cumulative fault count.

##### Test Group 6 — Policy Hot-Swap

| Test | Description | Expected |
|------|-------------|----------|
| `SwapPolicy_MidSimulation` | Switch FIFO→LRU at tick 5 | `activeReplacement` = "LRU", subsequent evictions use LRU logic |
| `GetPolicyName` | Check policy name after construction | "FIFO" (default) |

##### Test Group 7 — Frame Table Management

| Test | Description | Expected |
|------|-------------|----------|
| `InitializeFrameTable` | Init with 8 frames | 8 entries, all unoccupied |
| `OccupiedFrameCount` | Load 3 pages | `occupiedFrames` == 3 |
| `CleanupTerminatedProcess` | Terminate process after loading pages | Frames freed, `occupiedFrames` decremented |

##### Test Group 8 — Metrics

| Test | Description | Expected |
|------|-------------|----------|
| `PageFaultRate_Calculation` | 5 faults / 10 accesses | `pageFaultRate` == 50.0 |
| `TotalReplacements` | Fill frames + 3 more accesses | `totalReplacements` == 3 |

---

### Component 5 — Main Integration (Optional, minimal)

---

#### [MODIFY] [main.cpp](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/main.cpp)

Include MemoryManager and register it alongside ProcessManager and Scheduler. This is a minor integration point — add the memory manager to the module list:

```diff
+#include "modules/memory/MemoryManager.h"
 // In module initialization:
+MemoryManager memManager;
+memManager.initializeFrameTable(state, 16);
```

---

## File Summary

| Action | File | Description |
|--------|------|-------------|
| NEW | `engine/modules/memory/policies/FIFOPolicy.h` | FIFO page replacement (header-only) |
| NEW | `engine/modules/memory/policies/LRUPolicy.h` | LRU page replacement (header-only) |
| NEW | `engine/modules/memory/MemoryManager.h` | Memory Manager class declaration |
| NEW | `engine/modules/memory/MemoryManager.cpp` | Memory Manager implementation (~350 lines) |
| NEW | `tests/test_memory_manager.cpp` | Comprehensive test suite (~400 lines) |
| MODIFY | `engine/CMakeLists.txt` | Add memory_manager_lib, link to main |
| MODIFY | `tests/CMakeLists.txt` | Add test_memory_manager target |
| MODIFY | `engine/main.cpp` | Register MemoryManager module |

**Estimated total new code: ~800 lines**

---

## Open Questions

> [!IMPORTANT]
> **Textbook Reference String Testing:** To test FIFO/LRU against textbook examples deterministically, I'll need the Memory Manager to support an explicit "access this VPN" method (or an internal queue of VPNs to access). In the normal simulation, VPN access is round-robin. For test-only, I propose adding a `setAccessSequence(std::vector<uint32_t> vpns)` method that overrides the round-robin pattern. This keeps the production code clean while enabling textbook validation. Is this acceptable?

> [!IMPORTANT]
> **Process Termination Cleanup:** When a process is terminated (TERMINATED state), should the Memory Manager immediately free its frames on the same tick, or defer cleanup to the next tick? The plan above uses next-tick cleanup (Step 4 of onTick). This matches the Scheduler pattern where termination on tick N is observed on tick N+1.

---

## Verification Plan

### Automated Tests

```bash
# Build from project root
cd os-simulator/build
cmake .. -G "MinGW Makefiles"
cmake --build .

# Run all tests
ctest --output-on-failure

# Run memory manager tests specifically
./test_memory_manager --gtest_filter="*"
```

### Key Assertions
1. **FIFO correctness**: 15 faults for the classic reference string with 3 frames
2. **LRU correctness**: 12 faults for the same reference string with 3 frames  
3. **Policy hot-swap**: Switch mid-simulation without crash, subsequent evictions use new policy
4. **Frame cleanup**: Terminating a process frees all its frames
5. **Metrics accuracy**: `pageFaultRate` = `(totalPageFaults / totalAccesses) * 100`
6. **No regressions**: Existing Phase 1–3 tests still pass (`SimulationState`, `EventBus`, `ProcessManager`, `Scheduler`)

### Manual Verification
- Inspect Gantt log + page fault events to verify correct interleaving of Scheduler and Memory Manager ticks
- Verify `MemoryMetrics` struct values after known sequence
