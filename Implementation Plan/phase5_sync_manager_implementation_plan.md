# Phase 5 — Sync Manager Implementation Plan

## Goal

Implement the **Sync Manager** module — the fourth and final OS subsystem module for the MiniOS Kernel Simulator. This module provides process synchronization primitives (mutexes and semaphores), manages blocked queues, and includes a programmable race condition demonstration scenario.

> [!IMPORTANT]
> This builds on the foundation from Phases 1–4. The data structures (`Mutex`, `Semaphore`) and the `SyncPrimitiveType` enum already exist as stubs. The `SimulationState` already has `mutexTable`, `semaphoreTable`, and `blockedQueues` fields. The event type constants (`LOCK_ACQUIRED`, `LOCK_RELEASED`, `PROCESS_BLOCKED`, `PROCESS_UNBLOCKED`) are already defined in `SimEvent.h`.

---

## User Review Required

> [!WARNING]
> **Process State Interaction**: The Sync Manager will move processes between `RUNNING/READY → WAITING` (when blocked on a lock) and `WAITING → READY` (when unblocked). This overlaps with the Scheduler's state transition ownership. The plan below follows the SDD principle that modules communicate only through `SimulationState` — the Sync Manager modifies process state directly and publishes events. **The Scheduler must not schedule a process that is in WAITING state** (it already enforces this since it only dispatches from `readyQueue`).

> [!IMPORTANT]
> **Sync Request Model**: Since the simulator is tick-driven and processes don't execute real code, sync operations need a _request model_. The plan introduces a **`SyncRequest`** struct and a **`pendingSyncRequests`** queue in `SimulationState`. Each tick, the running process can have at most one pending sync request (acquire/release/wait/signal). For testing and the race condition demo, requests are injected programmatically via the SyncManager API. In the final system (Phase 7), the API Bridge will inject requests via REST.

---

## Proposed Changes

### Component 1 — Sync Request Data Structure

A new lightweight struct to represent a synchronization operation request from a process.

---

#### [NEW] [SyncRequest.h](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/modules/sync/SyncRequest.h)

```cpp
struct SyncRequest {
    int         pid;            // PID of the requesting process
    std::string operation;      // "ACQUIRE", "RELEASE", "WAIT", "SIGNAL"
    SyncPrimitiveType type;     // MUTEX, SEMAPHORE_BINARY, SEMAPHORE_COUNTING
    int         resourceId;     // mutexId or semId
};
```

**Fields**:
| Field | Type | Description |
|-------|------|-------------|
| `pid` | `int` | PID of process issuing the sync request |
| `operation` | `std::string` | One of: `"ACQUIRE"`, `"RELEASE"`, `"WAIT"`, `"SIGNAL"` |
| `type` | `SyncPrimitiveType` | Target primitive type |
| `resourceId` | `int` | ID of the target mutex or semaphore |

---

#### [MODIFY] [SimulationState.h](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/core/SimulationState.h)

Add the pending sync request queue to the Sync Manager section:

```diff
 // Sync Manager
 std::map<int, Mutex>          mutexTable;
 std::map<int, Semaphore>      semaphoreTable;
 std::map<int, std::deque<int>> blockedQueues;
+std::deque<SyncRequest>       pendingSyncRequests;  // queued sync operations
```

Also add `#include "modules/sync/SyncRequest.h"` to the includes, and clear `pendingSyncRequests` in `reset()`.

---

### Component 2 — SyncManager Module

The core module implementing `ISimModule`. Follows the exact same architectural pattern as `ProcessManager`, `Scheduler`, and `MemoryManager`.

---

#### [NEW] [SyncManager.h](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/modules/sync/SyncManager.h)

**Class design**:

```
SyncManager : public ISimModule
├── ISimModule interface
│   ├── onTick(state, bus)
│   ├── reset()
│   ├── getStatus()
│   └── getModuleName() → "SyncManager"
├── Primitive Creation API
│   ├── createMutex(state, bus, name) → mutexId
│   └── createSemaphore(state, bus, name, type, initialValue) → semId
├── Sync Request API (for tests / API Bridge)
│   ├── requestAcquire(state, pid, mutexId)
│   ├── requestRelease(state, pid, mutexId)
│   ├── requestWait(state, pid, semId)
│   └── requestSignal(state, pid, semId)
├── Race Condition Demo API
│   ├── setupRaceConditionDemo(state, bus)
│   └── isRaceConditionDemoActive()
└── Private per-tick helpers
    ├── processSyncRequests(state, bus)    — drain pendingSyncRequests queue
    ├── handleAcquire(state, bus, req)     — mutex acquire logic
    ├── handleRelease(state, bus, req)     — mutex release logic
    ├── handleWait(state, bus, req)        — semaphore wait() logic
    ├── handleSignal(state, bus, req)      — semaphore signal() logic
    └── cleanupTerminatedProcessLocks(state, bus) — release locks held by dead processes
```

**Private state**:
| Field | Type | Description |
|-------|------|-------------|
| `status_` | `ModuleStatus` | Module lifecycle status |
| `nextMutexId_` | `int` | Auto-incrementing mutex ID counter (starts at 1) |
| `nextSemId_` | `int` | Auto-incrementing semaphore ID counter (starts at 1) |
| `raceConditionDemoActive_` | `bool` | Whether the built-in race demo is running |
| `sharedCounter_` | `int` | Shared counter for race condition demonstration |
| `raceConditionLog_` | `vector<string>` | Log of accesses for race condition visibility |

---

#### [NEW] [SyncManager.cpp](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/modules/sync/SyncManager.cpp)

**`onTick()` Execution Sequence** (4 steps):

```
onTick(state, bus):
  1. processSyncRequests(state, bus)
     └─ Drain all SyncRequest entries from state.pendingSyncRequests
        └─ Route each to handleAcquire / handleRelease / handleWait / handleSignal

  2. cleanupTerminatedProcessLocks(state, bus)
     └─ For each terminated process:
          - Release any mutexes they own (auto-release)
          - Remove from all semaphore waiting queues

  3. updateBlockedQueues(state)
     └─ Rebuild state.blockedQueues from mutexTable and semaphoreTable waitingPids

  4. status_ = ModuleStatus::ACTIVE
```

---

**Detailed Logic per Operation**:

##### `handleAcquire(state, bus, req)` — Mutex Acquire

```
1. Look up mutex in state.mutexTable[req.resourceId]
2. IF mutex NOT found → log error, return
3. IF mutex.locked == false:
     a. mutex.locked = true
     b. mutex.ownerPid = req.pid
     c. mutex.lockedAtTick = state.currentTick
     d. mutex.totalAcquisitions++
     e. Publish LOCK_ACQUIRED event
     f. Log decision: "Process PID X acquired mutex 'name'"
4. ELSE (mutex is locked):
     a. mutex.waitingPids.push_back(req.pid)
     b. mutex.totalContentions++
     c. Set process state to WAITING: state.processTable[req.pid].state = WAITING
     d. Remove req.pid from state.readyQueue (if present)
     e. IF req.pid == state.runningPID → state.runningPID = -1
     f. Publish PROCESS_BLOCKED event
     g. Log decision: "Process PID X blocked on mutex 'name' (held by PID Y)"
```

##### `handleRelease(state, bus, req)` — Mutex Release

```
1. Look up mutex in state.mutexTable[req.resourceId]
2. IF mutex NOT found → log error, return
3. IF mutex.ownerPid != req.pid → log error (non-owner release), return
4. IF mutex.waitingPids is NOT empty:
     a. Pop front: nextPid = mutex.waitingPids.front(); mutex.waitingPids.pop_front()
     b. Transfer ownership: mutex.ownerPid = nextPid, mutex.lockedAtTick = currentTick
     c. mutex.totalAcquisitions++
     d. Set nextPid state to READY: state.processTable[nextPid].state = READY
     e. Push nextPid to back of state.readyQueue
     f. Publish LOCK_RELEASED event (for releasing process)
     g. Publish LOCK_ACQUIRED event (for newly-acquiring process)
     h. Publish PROCESS_UNBLOCKED event (for nextPid)
     i. Log decision: "PID X released mutex, PID Y unblocked and acquired"
5. ELSE (no waiters):
     a. mutex.locked = false
     b. mutex.ownerPid = -1
     c. mutex.lockedAtTick = 0
     d. Publish LOCK_RELEASED event
     e. Log decision: "PID X released mutex 'name' (no waiters)"
```

##### `handleWait(state, bus, req)` — Semaphore Wait (P / down)

```
1. Look up semaphore in state.semaphoreTable[req.resourceId]
2. IF semId NOT found → log error, return
3. sem.totalWaits++
4. IF sem.value > 0:
     a. sem.value--
     b. Log decision: "PID X wait() on semaphore 'name' succeeded (value now Y)"
5. ELSE (sem.value == 0):
     a. sem.waitingPids.push_back(req.pid)
     b. sem.totalBlocks++
     c. Set process state to WAITING
     d. Remove req.pid from readyQueue (if present)
     e. IF req.pid == state.runningPID → state.runningPID = -1
     f. Publish PROCESS_BLOCKED event
     g. Log decision: "PID X blocked on semaphore 'name' (value=0)"
```

##### `handleSignal(state, bus, req)` — Semaphore Signal (V / up)

```
1. Look up semaphore in state.semaphoreTable[req.resourceId]
2. IF semId NOT found → log error, return
3. sem.totalSignals++
4. IF sem.waitingPids is NOT empty:
     a. Pop front: nextPid = sem.waitingPids.front(); sem.waitingPids.pop_front()
     b. Set nextPid state to READY
     c. Push nextPid to back of readyQueue
     d. Publish PROCESS_UNBLOCKED event
     e. Log decision: "PID X signal() on semaphore, PID Y unblocked"
     // Note: value stays at 0 because the unblocked process "consumes" the signal
5. ELSE (no waiters):
     a. sem.value++ (but capped at initialValue for binary semaphores)
     b. Log decision: "PID X signal() on semaphore 'name' (value now Y)"
```

##### `cleanupTerminatedProcessLocks(state, bus)`

```
For each process in state.processTable where state == TERMINATED:
  1. For each mutex in mutexTable:
       IF mutex.ownerPid == terminatedPid → auto-release (same as handleRelease)
       Remove terminatedPid from mutex.waitingPids (if present)
  2. For each semaphore in semaphoreTable:
       Remove terminatedPid from sem.waitingPids (if present)
```

---

### Component 3 — Race Condition Demonstration

The race condition demo is a built-in scenario that shows data corruption **without** synchronization and correct behavior **with** synchronization (FR-SY-03, FR-SY-04).

**Built into `SyncManager` directly** (not a separate class):

#### `setupRaceConditionDemo(state, bus)` Method

1. Creates 2 processes: `"race_writer_1"` and `"race_writer_2"` (both CPU_BOUND, short bursts)
2. Creates 1 mutex: `"critical_section_lock"`
3. Sets `sharedCounter_ = 0`
4. Sets `raceConditionDemoActive_ = true`
5. When the demo is active, `onTick()` injects sync requests automatically:
   - **Without sync mode**: Both processes "increment" `sharedCounter_` every tick they run, with NO lock — results in interleaving and corrupted counter
   - **With sync mode**: Each process acquires the mutex before incrementing, releases after — counter is always correct

The demo logs each access to `raceConditionLog_` so the UI can show the exact interleaving pattern.

> [!NOTE]
> The race condition demo is a _conceptual demonstration_ within the simulation. Since all state mutations go through the single-threaded `onTick()` cycle, the "race" is simulated by deliberately interleaving reads and writes of the shared counter when processes alternate on the CPU without holding a lock. With the lock, only the lock-holder modifies the counter, producing the correct result.

---

### Component 4 — Build System Updates

---

#### [MODIFY] [CMakeLists.txt](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/CMakeLists.txt)

```diff
-# Phase 5: add_library(sync_manager_lib modules/sync/SyncManager.cpp)
+# ── Sync Manager Library (Phase 5) ─────────────────────────
+# Contains: SyncManager, Mutex, Semaphore, SyncRequest
+add_library(sync_manager_lib
+    modules/sync/SyncManager.cpp
+)
+target_include_directories(sync_manager_lib PUBLIC ${ENGINE_INCLUDE_DIR})
+target_link_libraries(sync_manager_lib PUBLIC core_lib)
```

Update the main executable link:
```diff
-target_link_libraries(os_simulator PRIVATE core_lib process_manager_lib scheduler_lib memory_manager_lib)
+target_link_libraries(os_simulator PRIVATE core_lib process_manager_lib scheduler_lib memory_manager_lib sync_manager_lib)
```

---

#### [MODIFY] [CMakeLists.txt](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/tests/CMakeLists.txt)

Add test target for the Sync Manager:

```diff
+# ── Test: SyncManager (Phase 5) ─────────────────────────────
+add_executable(test_sync_manager test_sync_manager.cpp)
+target_include_directories(test_sync_manager PRIVATE ${ENGINE_INCLUDE_DIR})
+target_link_libraries(test_sync_manager
+    PRIVATE
+    GTest::gtest_main
+    core_lib
+    process_manager_lib
+    sync_manager_lib
+)
+add_test(NAME SyncManagerTests COMMAND test_sync_manager)
```

---

### Component 5 — main.cpp Update

---

#### [MODIFY] [main.cpp](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/main.cpp)

- Add `#include "modules/sync/SyncManager.h"`
- Create `SyncManager` instance and register with `ClockController`
- Add a brief demo: create a mutex, two processes, show acquire/release/block/unblock
- Subscribe to `LOCK_ACQUIRED`, `LOCK_RELEASED`, `PROCESS_BLOCKED`, `PROCESS_UNBLOCKED` events

---

### Component 6 — Unit Test Suite

---

#### [NEW] [test_sync_manager.cpp](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/tests/test_sync_manager.cpp)

**Test fixture**: `SyncManagerTest` — creates `SimulationState`, `EventBus`, `ProcessManager`, `SyncManager`. Helper to create test processes.

**Test Cases** (organized by category):

##### A. Mutex Tests

| # | Test Name | What It Verifies |
|---|-----------|------------------|
| 1 | `MutexCreation` | `createMutex()` returns valid ID, mutex appears in `mutexTable` with correct defaults |
| 2 | `MutexAcquireUncontested` | Single process acquires unlocked mutex → `locked=true`, `ownerPid` set, `LOCK_ACQUIRED` event published |
| 3 | `MutexAcquireContested` | Second process tries to acquire held mutex → goes to `WAITING`, added to `waitingPids`, `PROCESS_BLOCKED` event |
| 4 | `MutexRelease_NoWaiters` | Owner releases mutex with no waiters → `locked=false`, `ownerPid=-1`, `LOCK_RELEASED` event |
| 5 | `MutexRelease_WithWaiters` | Owner releases, first waiter is unblocked → waiter moves to `READY`, gets ownership, `PROCESS_UNBLOCKED` + `LOCK_ACQUIRED` events |
| 6 | `MutexRelease_NonOwner` | Non-owner attempts release → operation rejected, mutex state unchanged |
| 7 | `MutexMultipleWaiters_FIFO` | 3 processes contend → released in FIFO order of `waitingPids` |
| 8 | `MutexOwnerTerminated_AutoRelease` | Owner process terminates → `cleanupTerminatedProcessLocks` auto-releases, next waiter gets lock |
| 9 | `MutexMetrics` | Verify `totalAcquisitions` and `totalContentions` counts match expected values |

##### B. Semaphore Tests

| # | Test Name | What It Verifies |
|---|-----------|------------------|
| 10 | `SemaphoreCreation_Binary` | `createSemaphore()` with `SEMAPHORE_BINARY` → `value=1`, `initialValue=1` |
| 11 | `SemaphoreCreation_Counting` | `createSemaphore()` with `SEMAPHORE_COUNTING`, N=3 → `value=3`, `initialValue=3` |
| 12 | `SemaphoreWait_Available` | `wait()` when value > 0 → value decremented, process NOT blocked |
| 13 | `SemaphoreWait_Blocked` | `wait()` when value == 0 → process blocked, added to `waitingPids` |
| 14 | `SemaphoreSignal_NoWaiters` | `signal()` with empty waiting queue → value incremented |
| 15 | `SemaphoreSignal_WithWaiters` | `signal()` with waiter → first waiter unblocked, value stays 0 |
| 16 | `BinarySemaphore_ValueCap` | Signal on binary semaphore with value=1 → value stays at 1 (not >1) |
| 17 | `CountingSemaphore_MultipleSlots` | 3 processes `wait()` on sem(3) → all succeed without blocking; 4th blocks |
| 18 | `SemaphoreMetrics` | Verify `totalWaits`, `totalSignals`, `totalBlocks` counts |

##### C. Integration Tests

| # | Test Name | What It Verifies |
|---|-----------|------------------|
| 19 | `ProcessBlocked_RemovedFromReadyQueue` | When blocked → PID removed from `readyQueue` and not scheduled |
| 20 | `ProcessUnblocked_AddedToReadyQueue` | When unblocked → PID pushed to back of `readyQueue` |
| 21 | `RunningProcess_Blocked_CPUIdle` | Running process blocks on mutex → `runningPID = -1`, CPU becomes idle |
| 22 | `CleanupTerminated_RemovesFromAllQueues` | Terminated process removed from all `waitingPids` queues |
| 23 | `MultiTick_ProducerConsumer` | Classic producer-consumer with binary semaphore over 10 ticks |

##### D. Race Condition Demo Tests

| # | Test Name | What It Verifies |
|---|-----------|------------------|
| 24 | `RaceCondition_WithoutSync` | Shared counter shows incorrect (corrupted) value when accessed without lock |
| 25 | `RaceCondition_WithSync` | Shared counter shows correct value when protected by mutex |
| 26 | `RaceConditionLog_ShowsInterleaving` | Interleaving log shows distinct access patterns between synced/unsynced |

##### E. Event Publishing Tests

| # | Test Name | What It Verifies |
|---|-----------|------------------|
| 27 | `Event_LOCK_ACQUIRED_Published` | `LOCK_ACQUIRED` event has correct fields (tick, sourcePid, resourceId) |
| 28 | `Event_LOCK_RELEASED_Published` | `LOCK_RELEASED` event published on release |
| 29 | `Event_PROCESS_BLOCKED_Published` | `PROCESS_BLOCKED` event published when process enters WAITING |
| 30 | `Event_PROCESS_UNBLOCKED_Published` | `PROCESS_UNBLOCKED` event published when process moves to READY |
| 31 | `DecisionLog_Populated` | Decision log entries are written for every sync operation |

##### F. Edge Cases

| # | Test Name | What It Verifies |
|---|-----------|------------------|
| 32 | `AcquireInvalidMutex` | Requesting acquire on non-existent mutex ID → no crash, error logged |
| 33 | `WaitInvalidSemaphore` | Requesting wait on non-existent semaphore ID → no crash, error logged |
| 34 | `ResetClearsAllState` | `reset()` clears internal counters and sets status to IDLE |
| 35 | `ModuleNameAndStatus` | `getModuleName()` returns "SyncManager", status transitions correctly |

---

## File Summary

| Action | File Path | Purpose |
|--------|-----------|---------|
| **NEW** | `engine/modules/sync/SyncRequest.h` | Sync operation request struct |
| **NEW** | `engine/modules/sync/SyncManager.h` | SyncManager header — class declaration |
| **NEW** | `engine/modules/sync/SyncManager.cpp` | SyncManager implementation — all sync logic |
| **MODIFY** | `engine/core/SimulationState.h` | Add `pendingSyncRequests` field + include |
| **MODIFY** | `engine/CMakeLists.txt` | Add `sync_manager_lib` target + link to main |
| **MODIFY** | `engine/main.cpp` | Register SyncManager, add sync demo |
| **NEW** | `tests/test_sync_manager.cpp` | 35-test unit test suite |
| **MODIFY** | `tests/CMakeLists.txt` | Add `test_sync_manager` target |

---

## Open Questions

> [!IMPORTANT]
> **Thread-level sync**: The Data Dictionary defines `TCB.blockedOnSyncId` for thread-level blocking. Should the Sync Manager operate at the **process level only** (blocking/unblocking PIDs) for Phase 5, and defer thread-level granularity to a future phase? The current implementation plan blocks at the **process level** since that aligns with the PRD functional requirements (FR-SY-01 through FR-SY-05) and the process-centric scheduling model.

> [!NOTE]
> **Race condition demo trigger**: The demo will be triggerable via `setupRaceConditionDemo()`. In Phase 7, this will map to a REST endpoint or be part of the workload loader scenarios. For now, it's available via direct API call in tests and `main.cpp`.

---

## Deferred Items (For Future Phases)

> [!IMPORTANT]
> **The following items are explicitly deferred from Phase 5. Future agents building subsequent phases MUST read this section for context.**

### 1. Thread-Level Synchronization (Deferred to Phase 6+)

**What**: The Data Dictionary defines `TCB.blockedOnSyncId` (Section 4, field table) — an integer field on each Thread Control Block that tracks which mutex/semaphore a thread is blocked on. This implies thread-granularity sync operations where individual threads within a process can independently acquire/release locks.

**Why deferred**: Phase 5 operates at the **process level** only. The Scheduler dispatches processes (PIDs), not threads. Blocking/unblocking is done on entire processes. This is consistent with:
- PRD FR-SY-01 through FR-SY-05 — all specify "process" as the unit of blocking
- The current Scheduler (Phase 3) only manages `readyQueue` of PIDs
- The SDD §3.2 tick sequence — modules operate on processes

**What future phases must do**:
- When thread-level scheduling is implemented (if ever), the SyncManager should be extended to:
  - Accept TIDs in sync requests (in addition to PIDs)
  - Set `TCB.blockedOnSyncId` when a thread blocks on a primitive
  - Clear `TCB.blockedOnSyncId` when a thread is unblocked
  - Track `TCB.waitingTime` accumulation for blocked threads
- The `SyncRequest` struct would need a `tid` field (default -1 for process-level)

### 2. REST API Endpoints for Sync Operations (Deferred to Phase 7)

**What**: The API Bridge (Phase 7) will need REST endpoints to create mutexes/semaphores and submit sync requests from the React dashboard.

**Suggested endpoints** (for the Phase 7 agent):
- `POST /sync/mutex/create` — `{name: "lock_1"}` → creates mutex
- `POST /sync/semaphore/create` — `{name: "sem_1", type: "SEMAPHORE_BINARY", initialValue: 1}`
- `POST /sync/acquire` — `{pid: N, mutexId: M}` → queues acquire request
- `POST /sync/release` — `{pid: N, mutexId: M}` → queues release request
- `POST /sync/wait` — `{pid: N, semId: S}` → queues wait request
- `POST /sync/signal` — `{pid: N, semId: S}` → queues signal request
- `POST /sync/race-demo` — `{enabled: true/false}` → starts/stops race condition demo

**The SyncManager API** (`createMutex`, `requestAcquire`, etc.) is designed to be called directly by the API Bridge — no additional adapter needed.

### 3. Race Condition Demo as Workload Scenario (Deferred to Phase 7)

**What**: The race condition demo (`setupRaceConditionDemo()`) should be integrated into the `WorkloadLoader` (Phase 7) as a selectable scenario alongside `cpu_bound`, `io_bound`, and `mixed`.

**The Phase 7 agent should**: Add a `race_condition` scenario to the `WorkloadScenario` table that calls `SyncManager::setupRaceConditionDemo()`.

---

## Verification Plan

### Automated Tests

```bash
# From the build directory:
cd d:\study\Semester 4 Spring 2026\Operating System\CEP\os-simulator\build

# Reconfigure and build
cmake ..
cmake --build .

# Run ONLY the sync manager tests
ctest -R SyncManagerTests --verbose

# Run ALL tests to verify no regressions
ctest --verbose
```

**Pass criteria**:
- All 35 Sync Manager tests pass
- All pre-existing tests (SimulationState, EventBus, ProcessManager, Scheduler, MemoryManager) continue to pass
- Build completes with zero warnings

### Manual Verification

- Run the `os_simulator` executable and verify console output shows:
  - Mutex creation and acquisition events
  - Process blocking and unblocking flow
  - Race condition demo output (corrupted vs. correct counter values)
