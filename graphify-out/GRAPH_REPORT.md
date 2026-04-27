# Graph Report - .  (2026-04-20)

## Corpus Check
- 60 files · ~54,308 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 192 nodes · 322 edges · 33 communities detected
- Extraction: 67% EXTRACTED · 33% INFERRED · 0% AMBIGUOUS · INFERRED: 106 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- [[_COMMUNITY_Community 0|Community 0]]
- [[_COMMUNITY_Community 1|Community 1]]
- [[_COMMUNITY_Community 2|Community 2]]
- [[_COMMUNITY_Community 3|Community 3]]
- [[_COMMUNITY_Community 4|Community 4]]
- [[_COMMUNITY_Community 5|Community 5]]
- [[_COMMUNITY_Community 6|Community 6]]
- [[_COMMUNITY_Community 7|Community 7]]
- [[_COMMUNITY_Community 8|Community 8]]
- [[_COMMUNITY_Community 9|Community 9]]
- [[_COMMUNITY_Community 10|Community 10]]
- [[_COMMUNITY_Community 11|Community 11]]
- [[_COMMUNITY_Community 12|Community 12]]
- [[_COMMUNITY_Community 13|Community 13]]
- [[_COMMUNITY_Community 14|Community 14]]
- [[_COMMUNITY_Community 15|Community 15]]
- [[_COMMUNITY_Community 16|Community 16]]
- [[_COMMUNITY_Community 17|Community 17]]
- [[_COMMUNITY_Community 18|Community 18]]
- [[_COMMUNITY_Community 19|Community 19]]
- [[_COMMUNITY_Community 20|Community 20]]
- [[_COMMUNITY_Community 21|Community 21]]
- [[_COMMUNITY_Community 22|Community 22]]
- [[_COMMUNITY_Community 23|Community 23]]
- [[_COMMUNITY_Community 24|Community 24]]
- [[_COMMUNITY_Community 25|Community 25]]
- [[_COMMUNITY_Community 26|Community 26]]
- [[_COMMUNITY_Community 27|Community 27]]
- [[_COMMUNITY_Community 28|Community 28]]
- [[_COMMUNITY_Community 29|Community 29]]
- [[_COMMUNITY_Community 30|Community 30]]
- [[_COMMUNITY_Community 31|Community 31]]
- [[_COMMUNITY_Community 32|Community 32]]

## God Nodes (most connected - your core abstractions)
1. `publish()` - 18 edges
2. `main()` - 16 edges
3. `TEST_F()` - 15 edges
4. `createProcess()` - 11 edges
5. `TEST_F()` - 11 edges
6. `onTick()` - 10 edges
7. `TEST()` - 10 edges
8. `reset()` - 9 edges
9. `TEST()` - 9 edges
10. `TEST_F()` - 9 edges

## Surprising Connections (you probably didn't know these)
- `TEST_F()` --calls--> `createThread()`  [INFERRED]
  os-simulator\tests\test_process_manager.cpp → os-simulator\engine\modules\process\ProcessManager.cpp
- `main()` --calls--> `initializeFrameTable()`  [INFERRED]
  os-simulator\engine\main.cpp → os-simulator\engine\modules\memory\MemoryManager.cpp
- `main()` --calls--> `subscribe()`  [INFERRED]
  os-simulator\engine\main.cpp → os-simulator\engine\core\EventBus.cpp
- `main()` --calls--> `createProcess()`  [INFERRED]
  os-simulator\engine\main.cpp → os-simulator\engine\modules\process\ProcessManager.cpp
- `main()` --calls--> `getModuleCount()`  [INFERRED]
  os-simulator\engine\main.cpp → os-simulator\engine\core\ClockController.cpp

## Communities

### Community 0 - "Community 0"
Cohesion: 0.11
Nodes (31): blockProcess(), cleanupTerminatedProcessLocks(), createMutex(), createSemaphore(), getModuleName(), getRaceConditionCounter(), getRaceConditionExpectedValue(), getStatus() (+23 more)

### Community 1 - "Community 1"
Cohesion: 0.14
Nodes (26): allModulesRegistered(), ClockController(), ClockController::Impl, clockLoop(), ensureThreadsStarted(), getCompletedTick(), getModuleCount(), pause() (+18 more)

### Community 2 - "Community 2"
Cohesion: 0.15
Nodes (15): bootstrap(), cleanupTerminatedProcessPages(), clearReferenceBits(), createPolicy(), evictPage(), findFreeFrame(), getNextVPN(), initializeFrameTable() (+7 more)

### Community 3 - "Community 3"
Cohesion: 0.19
Nodes (13): admitNewProcesses(), autoAssignBurst(), autoAssignCpuSegment(), autoAssignIO(), autoAssignMemory(), autoGenerateName(), createProcess(), createThread() (+5 more)

### Community 4 - "Community 4"
Cohesion: 0.22
Nodes (11): publish(), checkPreemption(), decrementBurst(), dispatchProcess(), handleIOInitiation(), handleQuantumExpiry(), ISimModule(), logGanttEntry() (+3 more)

### Community 5 - "Community 5"
Cohesion: 0.22
Nodes (10): setAccessSequence(), createPolicy(), getActivePolicyName(), setPolicy(), MemoryManagerTest, TEST_F(), makeSpec(), runOneTick() (+2 more)

### Community 6 - "Community 6"
Cohesion: 0.24
Nodes (10): killProcess(), processStateFromString(), processTypeFromString(), replacementPolicyFromString(), schedulingPolicyFromString(), simModeFromString(), simStatusFromString(), syncPrimitiveTypeFromString() (+2 more)

### Community 7 - "Community 7"
Cohesion: 0.25
Nodes (7): EventBus(), getTickEvents(), subscribe(), subscribeAll(), unsubscribe(), makeEvent(), TEST()

### Community 8 - "Community 8"
Cohesion: 1.0
Nodes (0): 

### Community 9 - "Community 9"
Cohesion: 1.0
Nodes (0): 

### Community 10 - "Community 10"
Cohesion: 1.0
Nodes (0): 

### Community 11 - "Community 11"
Cohesion: 1.0
Nodes (0): 

### Community 12 - "Community 12"
Cohesion: 1.0
Nodes (0): 

### Community 13 - "Community 13"
Cohesion: 1.0
Nodes (0): 

### Community 14 - "Community 14"
Cohesion: 1.0
Nodes (0): 

### Community 15 - "Community 15"
Cohesion: 1.0
Nodes (0): 

### Community 16 - "Community 16"
Cohesion: 1.0
Nodes (0): 

### Community 17 - "Community 17"
Cohesion: 1.0
Nodes (0): 

### Community 18 - "Community 18"
Cohesion: 1.0
Nodes (0): 

### Community 19 - "Community 19"
Cohesion: 1.0
Nodes (0): 

### Community 20 - "Community 20"
Cohesion: 1.0
Nodes (0): 

### Community 21 - "Community 21"
Cohesion: 1.0
Nodes (0): 

### Community 22 - "Community 22"
Cohesion: 1.0
Nodes (0): 

### Community 23 - "Community 23"
Cohesion: 1.0
Nodes (0): 

### Community 24 - "Community 24"
Cohesion: 1.0
Nodes (0): 

### Community 25 - "Community 25"
Cohesion: 1.0
Nodes (0): 

### Community 26 - "Community 26"
Cohesion: 1.0
Nodes (0): 

### Community 27 - "Community 27"
Cohesion: 1.0
Nodes (0): 

### Community 28 - "Community 28"
Cohesion: 1.0
Nodes (0): 

### Community 29 - "Community 29"
Cohesion: 1.0
Nodes (0): 

### Community 30 - "Community 30"
Cohesion: 1.0
Nodes (0): 

### Community 31 - "Community 31"
Cohesion: 1.0
Nodes (0): 

### Community 32 - "Community 32"
Cohesion: 1.0
Nodes (0): 

## Knowledge Gaps
- **6 isolated node(s):** `ClockController::Impl`, `ClockControllerTest`, `EngineIntegrationTest`, `MemoryManagerTest`, `ProcessManagerTest` (+1 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **Thin community `Community 8`** (2 nodes): `ModuleSlot()`, `ClockController.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 9`** (2 nodes): `ModuleStatus()`, `ISimModule.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 10`** (2 nodes): `IReplacementPolicy()`, `IReplacementPolicy.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 11`** (2 nodes): `IReplacementPolicy()`, `FIFOPolicy.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 12`** (2 nodes): `IReplacementPolicy()`, `LRUPolicy.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 13`** (2 nodes): `ISchedulingPolicy()`, `ISchedulingPolicy.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 14`** (2 nodes): `ISchedulingPolicy()`, `FCFSPolicy.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 15`** (2 nodes): `PriorityPolicy.h`, `ISchedulingPolicy()`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 16`** (2 nodes): `RoundRobinPolicy.h`, `ISchedulingPolicy()`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 17`** (1 nodes): `ProcessSpec.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 18`** (1 nodes): `DecisionLogEntry.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 19`** (1 nodes): `SimEvent.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 20`** (1 nodes): `SimulationState.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 21`** (1 nodes): `FrameTableEntry.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 22`** (1 nodes): `MemoryMetrics.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 23`** (1 nodes): `PageTable.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 24`** (1 nodes): `PageTableEntry.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 25`** (1 nodes): `PCB.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 26`** (1 nodes): `ProcessSpec.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 27`** (1 nodes): `TCB.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 28`** (1 nodes): `GanttEntry.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 29`** (1 nodes): `SchedulingMetrics.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 30`** (1 nodes): `Mutex.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 31`** (1 nodes): `Semaphore.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 32`** (1 nodes): `SyncRequest.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `publish()` connect `Community 4` to `Community 0`, `Community 1`, `Community 2`, `Community 3`, `Community 6`, `Community 7`?**
  _High betweenness centrality (0.203) - this node is a cross-community bridge._
- **Why does `reset()` connect `Community 0` to `Community 1`, `Community 5`, `Community 6`, `Community 7`?**
  _High betweenness centrality (0.126) - this node is a cross-community bridge._
- **Why does `createProcess()` connect `Community 3` to `Community 0`, `Community 1`, `Community 4`, `Community 5`, `Community 6`?**
  _High betweenness centrality (0.111) - this node is a cross-community bridge._
- **Are the 17 inferred relationships involving `publish()` (e.g. with `runSingleTick()` and `initPageTablesForNewProcesses()`) actually correct?**
  _`publish()` has 17 INFERRED edges - model-reasoned connections that need verification._
- **Are the 15 inferred relationships involving `main()` (e.g. with `registerModule()` and `initializeFrameTable()`) actually correct?**
  _`main()` has 15 INFERRED edges - model-reasoned connections that need verification._
- **Are the 14 inferred relationships involving `TEST_F()` (e.g. with `createMutex()` and `requestAcquire()`) actually correct?**
  _`TEST_F()` has 14 INFERRED edges - model-reasoned connections that need verification._
- **Are the 5 inferred relationships involving `createProcess()` (e.g. with `main()` and `publish()`) actually correct?**
  _`createProcess()` has 5 INFERRED edges - model-reasoned connections that need verification._