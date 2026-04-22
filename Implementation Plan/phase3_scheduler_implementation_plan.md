# Phase 3 — CPU Scheduler Module: Implementation Plan

## Goal

Implement the CPU Scheduler — the second OS module. The Scheduler selects a process from the ready queue on each tick, dispatches it to the CPU (`RUNNING` state), manages burst decrements and time quantum enforcement, handles preemption, logs Gantt chart entries, and computes scheduling performance metrics. Three scheduling policies (FCFS, Round Robin, Priority) are wired via the Strategy pattern for runtime hot-swapping.

---

## Background & Dependencies

Phase 1 (Foundation) and Phase 2 (Process Manager) are **complete**. The following are already in place:

| Component | Status | Key Files |
|-----------|--------|-----------|
| `SimulationState` | ✅ Done | [SimulationState.h](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/core/SimulationState.h) |
| `EventBus` | ✅ Done | [EventBus.h](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/core/EventBus.h) / `.cpp` |
| `ISimModule` | ✅ Done | [ISimModule.h](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/core/ISimModule.h) |
| `SimEnums` | ✅ Done | [SimEnums.h](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/core/SimEnums.h) — includes `SchedulingPolicy` enum |
| `PCB` / `TCB` / `ProcessSpec` | ✅ Done | [PCB.h](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/modules/process/PCB.h) |
| `ProcessManager` | ✅ Done | [ProcessManager.cpp](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/modules/process/ProcessManager.cpp) — handles NEW→READY, I/O completions, waiting time |
| `GanttEntry` stub | ✅ Done | [GanttEntry.h](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/modules/scheduler/GanttEntry.h) |
| `SchedulingMetrics` stub | ✅ Done | [SchedulingMetrics.h](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/modules/scheduler/SchedulingMetrics.h) |
| `ISchedulingPolicy` stub | ✅ Done | [ISchedulingPolicy.h](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/modules/scheduler/ISchedulingPolicy.h) |
| Policies directory | ✅ Scaffolded | `engine/modules/scheduler/policies/` (only `.gitkeep`) |

> [!IMPORTANT]
> **Tick execution order matters.** Per SDD §3.2, each tick runs modules in this order:
> 1. **Process Manager** — admits NEW→READY, handles I/O completions, increments waiting time
> 2. **CPU Scheduler** — selects from ready queue, dispatches, decrements burst, handles quantum/preemption, logs Gantt
> 3. Memory Manager (Phase 4) / Sync Manager (Phase 5) — not yet implemented
>
> The Scheduler runs **after** the Process Manager on each tick. This means newly-arrived processes admitted during this tick are immediately available in the ready queue.

---

## Design Decision — I/O Burst Cycle Model (Resolved)

> [!IMPORTANT]
> **Deterministic CPU–I/O Burst Cycle Model adopted.** A new field `cpuSegmentLength` (uint32_t) is added to `PCB` and `ProcessSpec`. This defines how many CPU ticks a process runs before entering an I/O burst. The cycle is:
> ```
> [CPU segment] → [I/O burst] → [CPU segment] → [I/O burst] → ... → [final CPU segment] → TERMINATED
> ```
> This matches the Silberschatz CPU–I/O burst cycle model and produces 100% deterministic, testable Gantt charts. The Data Dictionary must be updated before code (per DD Rule #2).

> [!NOTE]
> **Auto-assignment by ProcessType:**
> - `CPU_BOUND`: `cpuSegmentLength = totalCpuBurst` (never enters I/O, `ioBurstDuration = 0`)
> - `IO_BOUND`: `cpuSegmentLength = 1–2` (very frequent I/O)
> - `MIXED`: `cpuSegmentLength = totalCpuBurst / 3` (moderate alternation)

> [!WARNING]
> **Preemptive Priority re-ordering:** When `PRIORITY_P` is active, a newly-arrived higher-priority process preempts the currently running process — even if that process just started in the current tick. The preempted process goes back to the front of the ready queue with its remaining burst preserved. This is the textbook preemptive priority behavior.

---

## Proposed Changes

### Scheduler Module — Core

#### [NEW] [Scheduler.h](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/modules/scheduler/Scheduler.h)

The scheduler module class. Implements `ISimModule`.

**Class `Scheduler`:**
```
class Scheduler : public ISimModule {
public:
    Scheduler();

    // ── ISimModule interface ──
    void onTick(SimulationState& state, EventBus& bus) override;
    void reset() override;
    ModuleStatus getStatus() const override;
    std::string getModuleName() const override;

    // ── Policy Management ──
    void setPolicy(const std::string& policyName);
    std::string getActivePolicyName() const;

private:
    ModuleStatus status_;
    std::unique_ptr<ISchedulingPolicy> activePolicy_;
    int previousRunningPID_;   // for context switch detection

    // ── Per-tick steps (called in order by onTick) ──
    void dispatchProcess(SimulationState& state, EventBus& bus);
    void decrementBurst(SimulationState& state, EventBus& bus);
    void handleQuantumExpiry(SimulationState& state, EventBus& bus);
    void handleIOInitiation(SimulationState& state, EventBus& bus);
    void checkPreemption(SimulationState& state, EventBus& bus);
    void logGanttEntry(SimulationState& state);
    void updateMetrics(SimulationState& state);

    // ── Helpers ──
    void contextSwitchTo(int newPID, SimulationState& state, EventBus& bus);
    ISchedulingPolicy* createPolicy(const std::string& name);
};
```

**`onTick()` Sequencing:**

```
1. handleIOInitiation()     — if running process needs I/O, move RUNNING→WAITING
2. handleQuantumExpiry()    — if RR quantum expired, preempt running process
3. checkPreemption()        — if PRIORITY_P, check for higher-priority arrivals
4. dispatchProcess()        — if CPU idle, select next from ready queue via policy
5. decrementBurst()         — decrement remainingBurst of running process
6. logGanttEntry()          — append GanttEntry for this tick
7. updateMetrics()          — recompute SchedulingMetrics
```

---

#### [NEW] [Scheduler.cpp](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/modules/scheduler/Scheduler.cpp)

Full implementation of all methods. Key logic details:

**`dispatchProcess()`:**
- If `state.runningPID == -1` (CPU idle) and `readyQueue` is non-empty:
  - Call `activePolicy_->selectNext(readyQueue, processTable)` to get the PID
  - Set `state.runningPID = selectedPID`
  - Set `pcb.state = ProcessState::RUNNING`
  - If `pcb.startTick == 0`, set `pcb.startTick = state.currentTick` (first dispatch)
  - Reset `pcb.quantumUsed = 0`
  - Detect context switch: if `selectedPID != previousRunningPID_`, publish `CONTEXT_SWITCH` event, increment `pcb.contextSwitches`, increment `state.metrics.totalContextSwitches`
  - Publish `CPU_SCHEDULED` event
  - Log decision: "Scheduler selected PID X (policy: FCFS, reason: first in queue)"

**`decrementBurst()`:**
- If `state.runningPID != -1`:
  - `pcb.remainingBurst--`
  - `pcb.quantumUsed++`
  - If `pcb.remainingBurst == 0`:
    - Process is finished → set `pcb.state = TERMINATED`, set `pcb.terminationTick`, compute `pcb.turnaroundTime`
    - Set `state.runningPID = -1`
    - Publish `PROCESS_TERMINATED` event

**`handleQuantumExpiry()` (Round Robin only):**
- If `activePolicy` is `ROUND_ROBIN` and `pcb.quantumUsed >= state.timeQuantum`:
  - Preempt: set `pcb.state = READY`, push back to ready queue
  - Set `state.runningPID = -1`
  - Publish `CONTEXT_SWITCH` event
  - Log decision: "Process PID X preempted — quantum expired (used X/Y ticks)"

**`handleIOInitiation()`:**
- Uses the deterministic CPU–I/O burst cycle model:
  - If `pcb.ioBurstDuration > 0` AND `pcb.remainingBurst > 0` AND `pcb.quantumUsed >= pcb.cpuSegmentLength`:
    - Set `pcb.state = WAITING`, `pcb.ioRemainingTicks = pcb.ioBurstDuration`
    - Reset `pcb.quantumUsed = 0` (for next CPU segment)
    - Set `state.runningPID = -1`
    - Publish `PROCESS_STATE_CHANGED` event
    - Log decision: "Process PID X entering I/O burst (ran X/Y CPU segment ticks)"

**`checkPreemption()` (Priority Preemptive only):**
- If  `PRIORITY_P` and `state.runningPID != -1`:
  - Scan `readyQueue` for any PID with `priority < runningProcess.priority`
  - If found: preempt running process (move to READY, push to front of queue), dispatch the higher-priority process

**`updateMetrics()`:**
- After all dispatch/burst logic:
  - Count TERMINATED processes → `completedProcesses`
  - Sum `waitingTime / completedProcesses` → `avgWaitingTime`
  - Sum `turnaroundTime / completedProcesses` → `avgTurnaroundTime`
  - `cpuUtilization = (busyTicks / currentTick) * 100` where `busyTicks` = number of Gantt entries with `pid != -1`
  - `throughput = (completedProcesses / currentTick) * 100`
  - `totalProcesses` = total entries in `processTable`

---

### Scheduling Policies — Strategy Implementations

#### [NEW] [FCFSPolicy.h](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/modules/scheduler/policies/FCFSPolicy.h)

```cpp
class FCFSPolicy : public ISchedulingPolicy {
public:
    int selectNext(std::deque<int>& readyQueue,
                   const std::map<int, PCB>& processTable) override;
    std::string policyName() const override { return "FCFS"; }
};
```

**`selectNext()` logic:**
- Simply pop the front of the ready queue (FIFO order — processes arrive in order of `arrivalTick`)
- Sort ready queue by `arrivalTick` (just in case order was disturbed by priority preemption)
- Non-preemptive: once a process is dispatched, it runs until completion or I/O

---

#### [NEW] [RoundRobinPolicy.h](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/modules/scheduler/policies/RoundRobinPolicy.h)

```cpp
class RoundRobinPolicy : public ISchedulingPolicy {
public:
    int selectNext(std::deque<int>& readyQueue,
                   const std::map<int, PCB>& processTable) override;
    std::string policyName() const override { return "ROUND_ROBIN"; }
};
```

**`selectNext()` logic:**
- Pop the front of the ready queue (FIFO — processes returned after quantum expiry go to back)
- The quantum check is handled in `Scheduler::handleQuantumExpiry()`, not in the policy itself

---

#### [NEW] [PriorityPolicy.h](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/modules/scheduler/policies/PriorityPolicy.h)

```cpp
class PriorityPolicy : public ISchedulingPolicy {
public:
    explicit PriorityPolicy(bool preemptive);

    int selectNext(std::deque<int>& readyQueue,
                   const std::map<int, PCB>& processTable) override;
    std::string policyName() const override;

    bool isPreemptive() const { return preemptive_; }

private:
    bool preemptive_;
};
```

**`selectNext()` logic:**
- Scan the ready queue for the process with the **lowest `priority` value** (highest priority)
- Tie-breaking: FCFS order (earlier `arrivalTick` wins)
- Remove the selected PID from wherever it is in the deque (not necessarily front)
- The preemptive flag is used by the Scheduler's `checkPreemption()`, not by `selectNext()` itself

---

### Build System Updates

#### [MODIFY] [CMakeLists.txt](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/CMakeLists.txt)

Add the scheduler library:

```cmake
# ── Scheduler Library (Phase 3) ──────────────────────────────
add_library(scheduler_lib
    modules/scheduler/Scheduler.cpp
)
target_include_directories(scheduler_lib PUBLIC ${ENGINE_INCLUDE_DIR})
target_link_libraries(scheduler_lib PUBLIC core_lib)
```

Update the main executable to link against `scheduler_lib`:

```cmake
target_link_libraries(os_simulator PRIVATE core_lib process_manager_lib scheduler_lib)
```

---

#### [MODIFY] [CMakeLists.txt](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/tests/CMakeLists.txt)

Add new test executable:

```cmake
# ── Test: Scheduler (Phase 3) ───────────────────────────────
add_executable(test_scheduler test_scheduler.cpp)
target_include_directories(test_scheduler PRIVATE ${ENGINE_INCLUDE_DIR})
target_link_libraries(test_scheduler
    PRIVATE
    GTest::gtest_main
    core_lib
    process_manager_lib
    scheduler_lib
)
add_test(NAME SchedulerTests COMMAND test_scheduler)
```

---

### Main Harness Update

#### [MODIFY] [main.cpp](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/main.cpp)

Add scheduler instantiation and tick integration so the manual harness exercises the scheduler alongside the process manager.

---

### Process Manager Updates (for cpuSegmentLength)

#### [MODIFY] [PCB.h](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/modules/process/PCB.h)

Add `cpuSegmentLength` field (uint32_t) to the CPU Burst section. Initialize to `1` in the default constructor.

#### [MODIFY] [ProcessSpec.h](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/modules/process/ProcessSpec.h)

Add `cpuSegmentLength` field (uint32_t, default 0 = auto-assign).

#### [MODIFY] [ProcessManager.cpp](file:///d:/study/Semester%204%20Spring%202026/Operating%20System/CEP/os-simulator/engine/modules/process/ProcessManager.cpp)

Update `createProcess()` to auto-assign `cpuSegmentLength` based on `ProcessType`:
- `CPU_BOUND` → `totalCpuBurst` (no I/O)
- `IO_BOUND` → `1` or `2`
- `MIXED` → `max(1, totalCpuBurst / 3)`

Add new helper: `uint32_t autoAssignCpuSegment(ProcessType type, uint32_t totalBurst)`.

---

## File Summary

| Action | File | Purpose |
|--------|------|---------|
| **NEW** | `engine/modules/scheduler/Scheduler.h` | Scheduler class declaration |
| **NEW** | `engine/modules/scheduler/Scheduler.cpp` | Full scheduler `onTick()` implementation |
| **NEW** | `engine/modules/scheduler/policies/FCFSPolicy.h` | FCFS strategy (header-only) |
| **NEW** | `engine/modules/scheduler/policies/RoundRobinPolicy.h` | Round Robin strategy (header-only) |
| **NEW** | `engine/modules/scheduler/policies/PriorityPolicy.h` | Priority strategy — both NP and P (header-only) |
| **MODIFY** | `engine/modules/process/PCB.h` | Add `cpuSegmentLength` field |
| **MODIFY** | `engine/modules/process/ProcessSpec.h` | Add `cpuSegmentLength` field |
| **MODIFY** | `engine/modules/process/ProcessManager.cpp` | Auto-assign `cpuSegmentLength` in `createProcess()` |
| **MODIFY** | `engine/CMakeLists.txt` | Add `scheduler_lib` target, link to main |
| **MODIFY** | `tests/CMakeLists.txt` | Add `test_scheduler` target |
| **MODIFY** | `engine/main.cpp` | Wire scheduler into manual test harness |
| **NEW** | `tests/test_scheduler.cpp` | Comprehensive unit tests |

---

## Unit Test Plan — `test_scheduler.cpp`

The test suite verifies correctness against **hand-computed textbook examples**. Each test creates processes with specific burst/arrival/priority values, runs them through ticks, and asserts exact outcomes.

### Test Cases

#### 1. FCFS — Basic Ordering
| Process | Arrival | Burst |
|---------|---------|-------|
| P1 | 0 | 4 |
| P2 | 1 | 3 |
| P3 | 2 | 2 |

**Expected Gantt:** `P1 P1 P1 P1 P2 P2 P2 P3 P3`
**Expected metrics:**
- Avg Waiting Time: (0 + 3 + 5) / 3 = 2.67
- Avg Turnaround: (4 + 6 + 7) / 3 = 5.67
- CPU Utilization: 100%

#### 2. Round Robin — Quantum = 2
| Process | Arrival | Burst |
|---------|---------|-------|
| P1 | 0 | 5 |
| P2 | 0 | 3 |
| P3 | 0 | 1 |

**Expected Gantt:** `P1 P1 P2 P2 P3 P1 P1 P2 P1`
**Expected:** All processes cycle through with proper preemption at quantum boundaries.

#### 3. Round Robin — Quantum = 4
| Process | Arrival | Burst |
|---------|---------|-------|
| P1 | 0 | 6 |
| P2 | 0 | 4 |
| P3 | 0 | 2 |

**Expected Gantt:** `P1 P1 P1 P1 P2 P2 P2 P2 P3 P3 P1 P1`

#### 4. Priority Non-Preemptive
| Process | Arrival | Burst | Priority |
|---------|---------|-------|----------|
| P1 | 0 | 6 | 3 |
| P2 | 1 | 4 | 1 (highest) |
| P3 | 2 | 2 | 2 |

**Expected Gantt:** `P1 P1 P1 P1 P1 P1 P2 P2 P2 P2 P3 P3` (P1 runs to completion since non-preemptive, then P2, then P3)

#### 5. Priority Preemptive
| Process | Arrival | Burst | Priority |
|---------|---------|-------|----------|
| P1 | 0 | 6 | 3 |
| P2 | 2 | 4 | 1 (highest) |
| P3 | 4 | 2 | 2 |

**Expected Gantt:** `P1 P1 P2 P2 P2 P2 P3 P3 P1 P1 P1 P1` (P2 preempts P1 at tick 2, P3 cannot preempt P2, after P2 finishes P3 runs then P1 resumes)

#### 6. Policy Hot-Swap Mid-Simulation
- Start with FCFS, run 3 ticks
- Swap to ROUND_ROBIN mid-run
- Verify Gantt entries show policy name changes
- Verify ready queue behavior changes

#### 7. CPU Idle Handling
- No processes in ready queue
- Gantt should show `pid = -1`
- `cpuUtilization` should reflect idle periods

#### 8. Process Completion & Metrics
- Verify `terminationTick`, `turnaroundTime`, `waitingTime` on each PCB after all processes terminate
- Verify `SchedulingMetrics` aggregates match per-process values

#### 9. Context Switch Counting
- Verify `contextSwitches` per PCB increments correctly
- Verify `totalContextSwitches` in metrics equals sum

#### 10. Single Process (Edge Case)
- Single process, burst = 3
- Expected: runs for 3 ticks, no context switches, 0 waiting time

---

> [!NOTE]
> **Non-preemptive FCFS behavior:** In FCFS mode, once a process is dispatched, it will NOT be preempted even if new processes arrive. The only way it relinquishes the CPU is by completing its burst or entering an I/O wait. This matches textbook FCFS behavior.

---

## Verification Plan

### Automated Tests

```bash
cd os-simulator/build
cmake .. -DBUILD_TESTS=ON
cmake --build .
ctest --output-on-failure
```

All 4 test suites must pass:
- `SimulationStateTests` (Phase 1)
- `EventBusTests` (Phase 1)
- `ProcessManagerTests` (Phase 2)
- **`SchedulerTests` (Phase 3)** ← NEW

### Manual Verification

1. Run `os_simulator.exe` and verify console output shows:
   - Processes being scheduled in correct order per active policy
   - Gantt entries being logged
   - Metrics being computed
   - Context switches being tracked
2. Verify FCFS textbook example metrics match hand computation within 1% margin (NFR-05)
