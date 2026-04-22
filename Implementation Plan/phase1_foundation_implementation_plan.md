# Phase 1 — Foundation Implementation Plan

## Goal

Set up the complete project scaffold and implement all foundational infrastructure for the Mini OS Kernel Simulator: the directory structure, CMake build system, every enum, every data structure, the shared `SimulationState`, the `EventBus`, and all abstract interfaces (`ISimModule`, `ISchedulingPolicy`, `IReplacementPolicy`). After Phase 1, any team member can independently implement any OS module without touching foundation code.

## Background & Rationale

Phase 1 is the **zero-dependency foundation** of the entire project. Every OS module (Process Manager, Scheduler, Sync Manager, Memory Manager) depends on the types and interfaces defined here. Getting this wrong forces cascading refactors across the entire codebase. Getting this right means all four modules can be developed in parallel.

The SDD (Section 1.3) mandates: *"Every module boundary, interface contract, and data flow documented here must remain stable throughout the project."* — Phase 1 is where we lock those down.

---

## User Review Required

> [!IMPORTANT]
> **C++ Standard**: The SDD specifies `std::barrier` (C++20) for tick synchronisation, but lists C++17 as the standard. `std::barrier` requires C++20. This plan uses **C++20** to satisfy the SDD's concurrency model. Please confirm this is acceptable.

> [!WARNING]
> **DecisionLogEntry**: The `SimulationState` struct in the SDD references `std::vector<DecisionLogEntry> decisionLog`, but `DecisionLogEntry` is **not defined in the Data Dictionary**. The WebSocket packet shows `decision_log: [{tick, message}]`. This plan defines it as a 2-field struct (`tick: uint64_t`, `message: std::string`). The Data Dictionary document should be updated to include it (per Data Dictionary Rule #2).

> [!IMPORTANT]
> **Dependency Management**: Crow, websocketpp, and nlohmann/json are needed by the API Bridge (Phase 7), not Phase 1. This plan includes them in CMake as `FetchContent` declarations so they're ready when needed, but Phase 1 code does not use them. Confirm you want them fetched now or deferred.

---

## Proposed Changes

### Project Scaffold

Summary: Create the complete repository directory structure as defined in [SDD Section 9.2](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/SDD_MiniOS_Simulator.md).

#### [NEW] Complete Directory Tree

```
os-simulator/
├── engine/
│   ├── core/
│   │   ├── SimEnums.h                  # All 8 enumerations
│   │   ├── SimulationState.h           # Central shared state struct
│   │   ├── EventBus.h                  # Event bus declaration
│   │   ├── EventBus.cpp                # Event bus implementation
│   │   ├── ISimModule.h                # Abstract module interface
│   │   ├── ClockController.h           # Clock controller declaration (stub)
│   │   └── ClockController.cpp         # Clock controller (stub for Phase 1)
│   ├── modules/
│   │   ├── process/                    # Process Manager (empty, for Phase 2)
│   │   │   └── .gitkeep
│   │   ├── scheduler/                  # Scheduler (empty, for Phase 3)
│   │   │   ├── policies/
│   │   │   └── .gitkeep
│   │   ├── sync/                       # Sync Manager (empty, for Phase 5)
│   │   │   └── .gitkeep
│   │   └── memory/                     # Memory Manager (empty, for Phase 4)
│   │       ├── policies/
│   │       └── .gitkeep
│   ├── bridge/                         # API Bridge (empty, for Phase 7)
│   │   └── .gitkeep
│   ├── main.cpp                        # Engine entry point (minimal)
│   └── CMakeLists.txt                  # CMake build configuration
├── dashboard/                          # React Frontend (empty, for Phase 8)
│   └── .gitkeep
├── tests/
│   ├── test_simulation_state.cpp       # Tests for SimulationState
│   ├── test_event_bus.cpp              # Tests for EventBus
│   └── CMakeLists.txt                  # Test build configuration
├── docs/                               # Move existing docs here
│   ├── PRD_MiniOS_Simulator.md
│   ├── SDD_MiniOS_Simulator.md
│   └── DataDictionary_MiniOS_Simulator.md
└── CMakeLists.txt                      # Root CMake file
```

---

### Core Enumerations

Summary: Define all 8 enums exactly as specified in [Data Dictionary Section 2](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/DataDictionary_MiniOS_Simulator.md). Every enum lives in a single file.

#### [NEW] [SimEnums.h](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/core/SimEnums.h)

All enumerations from Data Dictionary §2.1–§2.8:

| Enum | Values | DD Section |
|------|--------|------------|
| `ProcessState` | `NEW`, `READY`, `RUNNING`, `WAITING`, `TERMINATED` | §2.1 |
| `ThreadState` | `T_NEW`, `T_RUNNABLE`, `T_RUNNING`, `T_BLOCKED`, `T_TERMINATED` | §2.2 |
| `SimMode` | `STEP`, `AUTO` | §2.3 |
| `SimStatus` | `IDLE`, `RUNNING`, `PAUSED`, `STOPPED` | §2.4 |
| `SchedulingPolicy` | `FCFS`, `ROUND_ROBIN`, `PRIORITY_NP`, `PRIORITY_P` | §2.5 |
| `ReplacementPolicy` | `FIFO`, `LRU` | §2.6 |
| `ProcessType` | `CPU_BOUND`, `IO_BOUND`, `MIXED` | §2.7 |
| `SyncPrimitiveType` | `MUTEX`, `SEMAPHORE_BINARY`, `SEMAPHORE_COUNTING` | §2.8 |

Implementation details:
- Use `enum class` (scoped enums) for type safety
- Add `std::string toString(<EnumType>)` helper function for each enum (needed for JSON serialisation)
- Add `<EnumType> fromString(const std::string&)` parser for each enum (needed for REST deserialization)
- Include header guards and `#pragma once`

---

### Data Structure Headers

Summary: Define all 12 structs from the Data Dictionary, each in its own header file within the module that owns it. **Every field name, type, and count must match the Data Dictionary exactly.**

#### [NEW] PCB.h — `engine/modules/process/PCB.h`
- Data Dictionary §3 — 17 fields + `threadIds` vector
- Struct: `PCB` with fields: `pid`, `name`, `type`, `priority`, `state`, `arrivalTick`, `startTick`, `terminationTick`, `totalCpuBurst`, `remainingBurst`, `quantumUsed`, `ioBurstDuration`, `ioRemainingTicks`, `ioCompletionTick`, `memoryRequirement`, `pageTableId`, `waitingTime`, `turnaroundTime`, `contextSwitches`, `pageFaultCount`, `threadIds`
- Include default constructor that sets `pid = -1`, `state = ProcessState::NEW`, `pageTableId = -1`, `startTick = 0`, `terminationTick = 0`

#### [NEW] TCB.h — `engine/modules/process/TCB.h`
- Data Dictionary §4 — 10 fields
- Struct: `TCB` with fields: `tid`, `parentPid`, `state`, `creationTick`, `stackSize`, `simulatedSP`, `cpuBurst`, `remainingBurst`, `blockedOnSyncId`, `waitingTime`
- Default: `blockedOnSyncId = -1`, `state = ThreadState::T_NEW`

#### [NEW] GanttEntry.h — `engine/modules/scheduler/GanttEntry.h`
- Data Dictionary §5.1 — 3 fields
- Struct: `GanttEntry` with fields: `tick`, `pid`, `policySnapshot`

#### [NEW] SchedulingMetrics.h — `engine/modules/scheduler/SchedulingMetrics.h`
- Data Dictionary §5.2 — 7 fields
- Struct: `SchedulingMetrics` with fields: `avgWaitingTime`, `avgTurnaroundTime`, `cpuUtilization`, `totalContextSwitches`, `throughput`, `completedProcesses`, `totalProcesses`
- Default: all fields = 0

#### [NEW] Mutex.h — `engine/modules/sync/Mutex.h`
- Data Dictionary §6.1 — 8 fields
- Struct: `Mutex` with fields: `mutexId`, `name`, `locked`, `ownerPid`, `waitingPids`, `lockedAtTick`, `totalAcquisitions`, `totalContentions`
- Default: `locked = false`, `ownerPid = -1`, `lockedAtTick = 0`

#### [NEW] Semaphore.h — `engine/modules/sync/Semaphore.h`
- Data Dictionary §6.2 — 9 fields
- Struct: `Semaphore` with fields: `semId`, `name`, `primitiveType`, `value`, `initialValue`, `waitingPids`, `totalWaits`, `totalSignals`, `totalBlocks`

#### [NEW] PageTableEntry.h — `engine/modules/memory/PageTableEntry.h`
- Data Dictionary §7.1 — 7 fields
- Struct: `PageTableEntry` with fields: `virtualPageNumber`, `frameNumber`, `valid`, `dirty`, `referenced`, `loadTick`, `lastAccessTick`
- Default: `frameNumber = -1`, `valid = false`, `dirty = false`, `referenced = false`, `loadTick = 0`, `lastAccessTick = 0`

#### [NEW] PageTable.h — `engine/modules/memory/PageTable.h`
- Data Dictionary §7.2 — 3 fields
- Struct: `PageTable` with fields: `ownerPid`, `pageSize`, `entries` (`std::vector<PageTableEntry>`)
- Default: `pageSize = 256`

#### [NEW] FrameTableEntry.h — `engine/modules/memory/FrameTableEntry.h`
- Data Dictionary §7.3 — 6 fields
- Struct: `FrameTableEntry` with fields: `frameNumber`, `occupied`, `ownerPid`, `virtualPageNumber`, `loadTick`, `lastAccessTick`
- Default: `occupied = false`, `ownerPid = -1`, `loadTick = 0`, `lastAccessTick = 0`

#### [NEW] MemoryMetrics.h — `engine/modules/memory/MemoryMetrics.h`
- Data Dictionary §7.4 — 6 fields
- Struct: `MemoryMetrics` with fields: `totalFrames`, `occupiedFrames`, `totalPageFaults`, `pageFaultRate`, `totalReplacements`, `activePolicy`
- Default: `totalFrames = 16`, all counters = 0, `activePolicy = "FIFO"`

#### [NEW] SimEvent.h — `engine/core/SimEvent.h`
- Data Dictionary §8.1 — 6 fields
- Struct: `SimEvent` with fields: `tick`, `eventType`, `sourcePid`, `targetPid`, `resourceId`, `description`
- Default: `sourcePid = -1`, `targetPid = -1`, `resourceId = -1`

#### [NEW] ProcessSpec.h — `engine/bridge/ProcessSpec.h`
- Data Dictionary §8.2 — 6 fields
- Struct: `ProcessSpec` with fields: `name`, `type`, `priority`, `cpuBurst`, `ioBurstDuration`, `memoryRequirement`
- Default: `priority = 5`, `cpuBurst = 0` (auto-assign), `ioBurstDuration = 0`, `memoryRequirement = 0`

#### [NEW] DecisionLogEntry.h — `engine/core/DecisionLogEntry.h`
- **Not in Data Dictionary** (see User Review above) — derived from SDD WebSocket packet
- 2 fields: `tick` (`uint64_t`), `message` (`std::string`)

---

### SimulationState

Summary: The central shared data store as defined in [SDD Section 4.2](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/SDD_MiniOS_Simulator.md). Aggregates all sub-structs into a single struct protected by `std::shared_mutex`.

#### [NEW] [SimulationState.h](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/core/SimulationState.h)

```cpp
struct SimulationState {
    // Clock
    uint64_t                      currentTick;
    SimMode                       mode;          // STEP | AUTO
    SimStatus                     status;        // IDLE | RUNNING | PAUSED | STOPPED
    uint32_t                      autoSpeedMs;   // tick interval in ms (default: 500)

    // Process Manager
    std::map<int, PCB>            processTable;
    std::map<int, TCB>            threadTable;
    int                           nextPID;       // starts at 1
    int                           nextTID;       // starts at 1

    // Scheduler
    std::deque<int>               readyQueue;    // PIDs
    int                           runningPID;    // -1 if idle
    std::string                   activePolicy;  // "FCFS" | "RR" | "PRIORITY"
    uint32_t                      timeQuantum;   // for RR (default: 2)
    std::vector<GanttEntry>       ganttLog;
    SchedulingMetrics             metrics;

    // Sync Manager
    std::map<int, Mutex>          mutexTable;
    std::map<int, Semaphore>      semaphoreTable;
    std::map<int, std::deque<int>> blockedQueues; // syncID -> blocked PIDs

    // Memory Manager
    std::vector<FrameTableEntry>  frameTable;    // FrameTable is a vector
    std::map<int, PageTable>      pageTables;    // PID -> PageTable
    std::string                   activeReplacement; // "FIFO" | "LRU"
    MemoryMetrics                 memMetrics;

    // Decision Log
    std::vector<DecisionLogEntry> decisionLog;

    // Synchronisation (not serialised to JSON)
    mutable std::shared_mutex     stateMutex;
};
```

Includes:
- Default constructor initialising: `currentTick = 0`, `mode = SimMode::STEP`, `status = SimStatus::IDLE`, `autoSpeedMs = 500`, `nextPID = 1`, `nextTID = 1`, `runningPID = -1`, `activePolicy = "FCFS"`, `timeQuantum = 2`, `activeReplacement = "FIFO"`
- `void reset()` method that clears all state back to defaults

---

### EventBus

Summary: Synchronous pub/sub mechanism as defined in [SDD Section 4.1](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/SDD_MiniOS_Simulator.md). All 13 event types supported.

#### [NEW] [EventBus.h](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/core/EventBus.h) + [EventBus.cpp](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/core/EventBus.cpp)

Design:
```cpp
class EventBus {
public:
    using EventHandler = std::function<void(const SimEvent&)>;

    // Subscribe to a specific event type, returns subscription ID
    int subscribe(const std::string& eventType, EventHandler handler);

    // Subscribe to ALL event types (used by API Bridge)
    int subscribeAll(EventHandler handler);

    // Unsubscribe by ID
    void unsubscribe(int subscriptionId);

    // Publish an event — invokes all matching handlers synchronously
    void publish(const SimEvent& event);

    // Get all events published during the current tick (for WebSocket broadcast)
    std::vector<SimEvent> getTickEvents() const;

    // Clear tick events (called at start of each new tick)
    void clearTickEvents();

    // Reset all subscriptions and events
    void reset();

private:
    std::mutex                                          mutex_;
    std::map<std::string, std::vector<std::pair<int, EventHandler>>> subscribers_;
    std::vector<std::pair<int, EventHandler>>           globalSubscribers_;
    std::vector<SimEvent>                               tickEvents_;
    int                                                 nextSubId_ = 0;
};
```

Supported event types (from SDD §4.1):
`PROCESS_CREATED`, `PROCESS_STATE_CHANGED`, `PROCESS_TERMINATED`, `CPU_SCHEDULED`, `CONTEXT_SWITCH`, `LOCK_ACQUIRED`, `LOCK_RELEASED`, `PROCESS_BLOCKED`, `PROCESS_UNBLOCKED`, `PAGE_FAULT`, `PAGE_REPLACED`, `TICK_ADVANCED`

> [!NOTE]
> The SDD lists 12 event types in the table (the 13th is implied by the "all modules" producer for TICK_ADVANCED). We define string constants for all of them to prevent typos.

---

### Abstract Interfaces

Summary: The three interface contracts from [SDD Sections 3.3–3.4](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/SDD_MiniOS_Simulator.md) that all modules and policies must implement.

#### [NEW] [ISimModule.h](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/core/ISimModule.h)

```cpp
// Status enum for module health reporting
enum class ModuleStatus {
    IDLE,       // Module initialised but simulation not started
    ACTIVE,     // Module processing ticks normally
    ERROR       // Module encountered an error
};

class ISimModule {
public:
    virtual void onTick(SimulationState& state, EventBus& bus) = 0;
    virtual void reset() = 0;
    virtual ModuleStatus getStatus() const = 0;
    virtual std::string getModuleName() const = 0;
    virtual ~ISimModule() = default;
};
```

#### [NEW] [ISchedulingPolicy.h](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/modules/scheduler/ISchedulingPolicy.h)

```cpp
class ISchedulingPolicy {
public:
    virtual int selectNext(std::deque<int>& readyQueue,
                           const std::map<int, PCB>& processTable) = 0;
    virtual std::string policyName() const = 0;
    virtual ~ISchedulingPolicy() = default;
};
```

> [!NOTE]
> The SDD shows `PCB* selectNext(ReadyQueue& queue)` but since the ready queue stores PIDs (integers) and PCBs live in the processTable map, the interface is adjusted to take both. The return type is `int` (PID of selected process, -1 if none).

#### [NEW] [IReplacementPolicy.h](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/modules/memory/IReplacementPolicy.h)

```cpp
class IReplacementPolicy {
public:
    virtual int selectVictimFrame(const std::vector<FrameTableEntry>& frames) = 0;
    virtual std::string policyName() const = 0;
    virtual ~IReplacementPolicy() = default;
};
```

---

### ClockController (Stub)

Summary: Minimal stub of the Clock Controller from [SDD Section 3.2](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/SDD_MiniOS_Simulator.md). Full implementation is Phase 6, but the class skeleton is needed now so `main.cpp` can compile.

#### [NEW] [ClockController.h](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/core/ClockController.h) + ClockController.cpp

```cpp
class ClockController {
public:
    ClockController(SimulationState& state, EventBus& bus);

    // Register an OS module to be called on each tick
    void registerModule(std::shared_ptr<ISimModule> module);

    // Advance one tick (Step Mode) — called manually
    void stepOnce();

    // Start/stop auto advancement
    void start();
    void pause();
    void reset();

private:
    SimulationState& state_;
    EventBus& bus_;
    std::vector<std::shared_ptr<ISimModule>> modules_;
};
```

Phase 1 implementation: `stepOnce()` iterates through registered modules and calls `onTick()` sequentially (single-threaded). Threading and barriers are added in Phase 6.

---

### CMake Build System

Summary: Set up CMake per [SDD Section 8](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/SDD_MiniOS_Simulator.md).

#### [NEW] Root `CMakeLists.txt`
- `cmake_minimum_required(VERSION 3.20)`
- `project(MiniOSSimulator LANGUAGES CXX)`
- `set(CMAKE_CXX_STANDARD 20)`
- `add_subdirectory(engine)`
- `add_subdirectory(tests)` (conditional on `BUILD_TESTS` option)

#### [NEW] `engine/CMakeLists.txt`
- Library target `core_lib`: `SimEnums.h`, `SimulationState.h`, `EventBus.cpp`, `ISimModule.h`, `SimEvent.h`, `DecisionLogEntry.h`
- Library target `clock_lib`: `ClockController.cpp`, links `core_lib`
- Header-only targets for data structs: `PCB.h`, `TCB.h`, `GanttEntry.h`, etc.
- Executable `os_simulator`: `main.cpp`, links all libraries
- `FetchContent` declarations for Crow, nlohmann/json (fetched but not linked yet)

#### [NEW] `tests/CMakeLists.txt`
- `FetchContent` Google Test
- `test_simulation_state` executable
- `test_event_bus` executable

---

### Entry Point

#### [NEW] `engine/main.cpp`

Minimal entry point that proves the foundation compiles and works:
```cpp
int main() {
    SimulationState state;
    EventBus bus;
    ClockController clock(state, bus);

    // Log that engine started
    std::cout << "Mini OS Kernel Simulator Engine v1.0" << std::endl;
    std::cout << "Status: " << toString(state.status) << std::endl;
    std::cout << "Mode: " << toString(state.mode) << std::endl;
    std::cout << "Tick: " << state.currentTick << std::endl;
    std::cout << "Foundation ready. No modules registered." << std::endl;

    return 0;
}
```

---

### Unit Tests (Phase 1)

#### [NEW] `tests/test_simulation_state.cpp`
- **Default initialisation test**: Verify all default values match SDD/DD spec
- **Reset test**: Populate state with data, call `reset()`, verify clean state
- **PCB creation test**: Create a PCB, insert into `processTable`, verify field values
- **TCB creation test**: Same for TCB
- **FrameTable test**: Create FrameTableEntries, verify defaults
- **PageTable test**: Create a PageTable with entries, verify structure

#### [NEW] `tests/test_event_bus.cpp`
- **Subscribe and publish test**: Subscribe to `PROCESS_CREATED`, publish event, verify handler called
- **Multiple subscribers test**: Two handlers on same event type, both called
- **Global subscriber test**: `subscribeAll()` receives all event types
- **Unsubscribe test**: Unsubscribed handler not called
- **Tick events test**: Published events appear in `getTickEvents()`, cleared by `clearTickEvents()`
- **Reset test**: All subscriptions and events cleared

---

## File Count Summary

| Category | New Files | Description |
|----------|-----------|-------------|
| Core headers | 7 | SimEnums, SimulationState, EventBus, ISimModule, SimEvent, DecisionLogEntry, ClockController |
| Core sources | 2 | EventBus.cpp, ClockController.cpp |
| Data struct headers | 10 | PCB, TCB, GanttEntry, SchedulingMetrics, Mutex, Semaphore, PageTableEntry, PageTable, FrameTableEntry, MemoryMetrics |
| Interface headers | 2 | ISchedulingPolicy, IReplacementPolicy |
| Bridge headers | 1 | ProcessSpec |
| Build files | 3 | Root CMakeLists, engine CMakeLists, tests CMakeLists |
| Entry point | 1 | main.cpp |
| Tests | 2 | test_simulation_state, test_event_bus |
| Git keeps | 6 | Placeholder files for empty module dirs |
| **Total** | **34 files** | |

---

## Open Questions

> [!IMPORTANT]
> **Q1**: The SDD says C++17 but uses `std::barrier` which is C++20. Should we use C++20, or should we implement a custom barrier in C++17? (Recommendation: use C++20 — it's a superset and we need `std::barrier` for Phase 6.)

> [!IMPORTANT]
> **Q2**: Should the `docs/` directory be a copy of the existing `.md` files or should we move them? Moving keeps the repo clean but changes file paths. (Recommendation: keep the docs in the root for now since the `.gemini` rules reference them there, and symlink or copy to `docs/` later.)

> [!WARNING]
> **Q3**: The Data Dictionary specifies `FrameTable` as a type in `SimulationState`, but doesn't define a `FrameTable` struct — only `FrameTableEntry`. This plan uses `std::vector<FrameTableEntry>` as the frame table type. Confirm this is acceptable, or should we add a `FrameTable` wrapper struct?

---

## Verification Plan

### Automated Tests
```bash
# Build everything
cd os-simulator
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON
cmake --build .

# Run tests
ctest --output-on-failure

# Run the engine binary (should print status and exit)
./engine/os_simulator
```

Expected output from `os_simulator`:
```
Mini OS Kernel Simulator Engine v1.0
Status: IDLE
Mode: STEP
Tick: 0
Foundation ready. No modules registered.
```

### Manual Verification
- Verify every struct field name matches the Data Dictionary using the `verify_data_dictionary` skill
- Verify the directory structure matches SDD §9.2
- Verify all enum values match Data Dictionary §2
- Confirm the project compiles with zero warnings (`-Wall -Wextra -Wpedantic`)
