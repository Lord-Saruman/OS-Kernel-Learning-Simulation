# Phase 6 - Clock Controller and Integration Implementation Plan

> [!NOTE]
> **Post-Implementation Amendments (April 2026):**
> After implementation review, the following changes were made to this plan and the codebase:
> 1. **STOPPED removed from SimStatus** — dead state never entered; simulator resets directly to IDLE.
> 2. **STEP-while-PAUSED enabled** — `requestStep()` now works when PAUSED (better UX for learning tool).
> 3. **EngineRuntimeConfig completed** — `autoSpeedMs` and `timeQuantum` are now preserved across reset.
> 4. **Nested lock ordering fixed** — `bus_.clearTickEvents()` moved outside `stateMutex` in `runSingleTick()`.
> 5. **MemoryManager coupling removed** — `ISimModule::bootstrap()` replaces the `dynamic_pointer_cast` pattern.
> 6. **Lock discipline documented** — dual-lock rules for `controlMutex` vs `stateMutex` added to `ClockController.h`.
> 7. **SDD updated** — §3.2 tick order, §4.2 SimStatus, §7.2-7.3 phased execution model all amended.
> 8. **Data Dictionary updated** — §2.4 SimStatus aligned with code.

## Goal

Replace the current Phase 1 sequential `ClockController` stub with a real integration runtime that can coordinate all four OS modules, support STEP and AUTO simulation modes, pause/resume/reset safely, and provide a stable per-tick event flow for Phase 7.

Phase 6 is complete when:

- the engine can run `ProcessManager`, `Scheduler`, `MemoryManager`, and `SyncManager` in one runtime
- ticks can advance in both STEP and AUTO modes without deadlocks or duplicate execution
- reset returns the simulator to a clean, reusable state
- end-to-end integration tests verify process admission, scheduling, memory access, synchronization, and event publication in the same run

> [!IMPORTANT]
> **PRD Coverage:** FR-SIM-01 through FR-SIM-04
> **Supporting Requirements:** FR-SC-04 through FR-SC-06, FR-SY-05, FR-MM-05, FR-MM-06, NFR-01, NFR-05, NFR-07
> **SDD Reference:** Section 3.2 (Simulation Clock), 4.1 (Event Bus), 4.2 (SimulationState), 7.1-7.3 (Concurrency Model)
> **Data Dictionary:** Sections 2.3 (`SimMode`), 2.4 (`SimStatus`), 8.1 (`SimEvent`), plus `SimulationState` clock fields

---

## User Review Required

> [!WARNING]
> **Execution Model Mismatch:** The SDD concurrency section describes all four module threads executing concurrently per tick, but the current Phase 2-5 code assumes deterministic ownership and ordering around shared fields such as `readyQueue`, `runningPID`, `processTable`, `blockedQueues`, and `decisionLog`.
>
> This plan uses a **phased worker-thread orchestration model**:
> - real worker threads still exist
> - `std::barrier` is still used
> - each tick runs in deterministic slots: `Process -> Scheduler -> Memory -> Sync`
> - only one module mutates `SimulationState` at a time
>
> This is the safest way to integrate Phases 1-5 without rewriting every module around intent queues or delta snapshots.

> [!IMPORTANT]
> **Reset Semantics:** `ClockController::reset()` currently calls `bus_.reset()`, which clears all subscribers. That is fine for a local demo, but it will break Phase 7 because the API Bridge should stay subscribed across `/sim/reset`. This plan changes reset behavior so it clears tick events and state, but **preserves subscriptions**.

> [!IMPORTANT]
> **Post-Reset Bootstrap:** `SimulationState::reset()` clears `frameTable`. If nothing recreates frames after reset, the Memory Manager cannot run correctly on the next simulation start. This plan requires the integration layer to reapply runtime bootstrap configuration after reset, especially frame-table initialization.

> [!IMPORTANT]
> **STEP Mode Semantics:** This plan assumes `/sim/start` puts the controller into `RUNNING`, and `/sim/step` only advances one tick when `mode == STEP` and `status == RUNNING`. If you want `step` to work while `PAUSED`, that should be decided now because it changes controller state logic and API behavior in Phase 7.

---

## Proposed Changes

### Component 1 - Define the Integration Execution Model

Phase 6 should formalize one tick as a controller-owned sequence, not just "call `onTick()` on everything somehow."

#### Chosen Tick Order

Each tick should execute in this exact order:

1. `ProcessManager`
2. `Scheduler`
3. `MemoryManager`
4. `SyncManager`
5. `TICK_ADVANCED` event publication

#### Why this order

- `ProcessManager` must admit `NEW -> READY` and complete I/O before the scheduler looks at `readyQueue`
- `Scheduler` must decide `runningPID` before memory access can be simulated
- `MemoryManager` depends on the final `runningPID` for the current tick
- `SyncManager` can process queued lock/semaphore operations after the current tick's CPU/memory activity and prepare blocked/unblocked state for the next tick

#### Important note

This order is slightly more specific than the SDD's narrative list, but it matches the actual contracts already encoded in the current module implementations and comments, especially the Scheduler's expectation that Process admission happens first.

---

### Component 2 - Redesign `ClockController` as a Real Runtime Coordinator

The current `ClockController` is still a Phase 1 stub:

- no worker threads
- no clock thread
- no barrier
- no condition variable
- no AUTO mode loop
- no safe shutdown lifecycle

Phase 6 converts it into the runtime coordinator for the engine.

#### [MODIFY] `os-simulator/engine/core/ClockController.h`

Expand the controller API to cover runtime lifecycle, mode changes, and tick requests:

```cpp
enum class ModuleSlot {
    PROCESS = 0,
    SCHEDULER = 1,
    MEMORY = 2,
    SYNC = 3
};

class ClockController {
public:
    ClockController(SimulationState& state, EventBus& bus);
    ~ClockController();

    void registerModule(ModuleSlot slot, std::shared_ptr<ISimModule> module);

    void start();
    void pause();
    void reset();
    void shutdown();

    void setMode(SimMode mode);
    void setAutoSpeedMs(uint32_t ms);
    bool requestStep();

    size_t getModuleCount() const;
    bool allModulesRegistered() const;
    uint64_t getCompletedTick() const;

private:
    void ensureThreadsStarted();
    void clockLoop();
    void workerLoop(ModuleSlot slot);
    void runSingleTick();
    void runPhase(ModuleSlot slot);
};
```

#### Internal state to add

- `std::array<std::shared_ptr<ISimModule>, 4> modules_`
- `std::array<std::thread, 4> workerThreads_`
- `std::thread clockThread_`
- `std::mutex controlMutex_`
- `std::condition_variable controlCv_`
- `std::condition_variable tickDoneCv_`
- `std::unique_ptr<std::barrier<>> phaseBarrier_`
- `ModuleSlot activeSlot_`
- `bool threadsStarted_`
- `bool shutdownRequested_`
- `bool stepRequested_`
- `bool tickInFlight_`
- `uint64_t completedTick_`

#### Design rules

- `ClockController` owns thread lifecycle
- modules still own their own logic and `reset()`
- controller owns tick order, wake-up policy, and safe stop/reset boundaries
- `SimulationState` mutation during ticks is controller-serialized by lock discipline, not left to modules

---

### Component 3 - Implement Barrier-Based Worker Threads

Use one worker thread per OS module, but do **not** let them mutate shared state simultaneously in Phase 6.

#### [MODIFY] `os-simulator/engine/core/ClockController.cpp`

##### Worker thread loop

Each worker thread should:

1. wait at the phase barrier for a phase to start
2. check whether `activeSlot_` matches its assigned slot
3. if yes, acquire `state.stateMutex` as a write lock and call `module->onTick(state, bus)`
4. wait at the phase barrier again to signal phase completion
5. repeat until shutdown

##### Controller tick loop

For each tick:

1. wait until the clock is allowed to run
2. clear prior tick events with `bus_.clearTickEvents()`
3. increment `state.currentTick`
4. execute `runPhase(PROCESS)`
5. execute `runPhase(SCHEDULER)`
6. execute `runPhase(MEMORY)`
7. execute `runPhase(SYNC)`
8. publish `TICK_ADVANCED`
9. mark `completedTick_`
10. notify waiters and loop again

##### `runPhase(slot)` behavior

`runPhase(slot)` should:

- set `activeSlot_ = slot`
- release the start barrier so all four workers wake up
- allow only the matching worker to run its `onTick()`
- block again on the completion barrier until that phase is finished

#### Why use phased workers instead of one controller thread calling modules directly?

- it fulfills the "real threads + barrier" direction of the architecture
- it keeps the controller ready for future refinement
- it avoids breaking Phase 2-5 module logic with unsafe parallel mutation

---

### Component 4 - Add STEP and AUTO Runtime Control

Phase 6 is the first phase that fully implements the simulator control requirements from the PRD.

#### STEP mode

`requestStep()` should:

- return `false` if `state.mode != SimMode::STEP`
- return `false` if `state.status != SimStatus::RUNNING`
- otherwise set `stepRequested_ = true`, notify `controlCv_`, and return `true`

The clock thread should consume exactly one pending step request and run exactly one tick.

#### AUTO mode

When `state.mode == SimMode::AUTO` and `state.status == SimStatus::RUNNING`, the clock thread should:

- sleep/wait for `state.autoSpeedMs`
- wake
- run one tick
- repeat until paused, reset, or shutdown

#### `start()`

`start()` should:

- lazily launch worker and clock threads on first use
- set `state.status = SimStatus::RUNNING`
- notify the clock thread

#### `pause()`

`pause()` should:

- set `state.status = SimStatus::PAUSED`
- prevent new ticks from starting
- allow the current in-flight tick to finish cleanly

#### `setMode()` and `setAutoSpeedMs()`

Add direct controller methods now so Phase 7 REST endpoints can call them without adding new engine-side plumbing later.

---

### Component 5 - Introduce Safe Reset and Shutdown Semantics

Reset is no longer just "clear some state." It must be safe while worker threads and the clock thread exist.

#### Reset sequence

`ClockController::reset()` should follow this order:

1. acquire `controlMutex_`
2. stop new tick generation
3. wait until `tickInFlight_ == false`
4. acquire `state.stateMutex`
5. call `state.reset()`
6. release `state.stateMutex`
7. call `module->reset()` on all registered modules
8. reapply runtime bootstrap configuration
9. clear tick events only
10. set `state.status = SimStatus::IDLE`

#### Critical change

Do **not** call `bus_.reset()` from `ClockController::reset()`.

Reason:

- reset should clear simulation state
- reset should not silently detach long-lived subscribers
- Phase 7 WebSocket broadcasting depends on subscriber continuity

#### Shutdown sequence

`shutdown()` should:

- set `shutdownRequested_ = true`
- wake the clock thread
- release both barrier checkpoints so workers can exit
- join all worker threads and the clock thread

The controller destructor should call `shutdown()` defensively.

---

### Component 6 - Reapply Runtime Bootstrap After Reset

The current project bootstraps some integration state manually in `main.cpp`. That becomes fragile once reset/restart is supported.

#### Required bootstrap items

- reinitialize memory frames
- restore the scheduler's active policy in `SimulationState`
- restore the memory replacement policy in `SimulationState`
- restore default `timeQuantum`
- restore default `autoSpeedMs`

#### Recommended approach

Keep a small runtime bootstrap configuration in the integration layer:

```cpp
struct EngineRuntimeConfig {
    uint32_t frameCount = 16;
    uint32_t autoSpeedMs = 500;
    uint32_t timeQuantum = 2;
};
```

This config is not part of the Data Dictionary or API payloads. It is internal engine wiring needed to make reset deterministic.

#### Practical implication

The integration layer must no longer rely on one-time setup in `main.cpp` alone. Whatever happens at startup must also happen after reset.

---

### Component 7 - Update `main.cpp` Into a Real Integration Harness

The current `main.cpp` is still a phase demo file:

- it does not register the `Scheduler`
- it hardcodes manual step calls
- it prints phase-specific demonstrations rather than exercising the fully integrated engine

#### [MODIFY] `os-simulator/engine/main.cpp`

Update `main.cpp` to:

1. create all core objects: `SimulationState`, `EventBus`, `ClockController`
2. instantiate all four modules:
   - `ProcessManager`
   - `Scheduler`
   - `MemoryManager`
   - `SyncManager`
3. register them explicitly by slot
4. initialize the frame table
5. create a small mixed workload
6. demonstrate:
   - STEP mode for a few ticks
   - mode switch to AUTO
   - pause
   - reset

#### Registration must become explicit

Use slot-based registration instead of "vector order means behavior" so Phase 6 does not depend on accidental ordering:

```cpp
clock.registerModule(ModuleSlot::PROCESS, procMgr);
clock.registerModule(ModuleSlot::SCHEDULER, scheduler);
clock.registerModule(ModuleSlot::MEMORY, memMgr);
clock.registerModule(ModuleSlot::SYNC, syncMgr);
```

This removes the current integration bug where the scheduler is missing entirely from `main.cpp`.

---

### Component 8 - Add a Dedicated Clock/Integration Test Suite

There is currently no test coverage for the controller itself. Phase 6 must add that coverage because concurrency bugs are rarely caught by module unit tests.

#### [NEW] `os-simulator/tests/test_clock_controller.cpp`

Primary controller tests:

1. `StepModeAdvancesExactlyOneTickPerRequest`
2. `AutoModeAdvancesWithoutManualStep`
3. `PauseStopsTickAdvancement`
4. `ResetClearsStateAndModules`
5. `ResetPreservesEventSubscribers`
6. `ShutdownExitsWithoutDeadlock`

#### [NEW] `os-simulator/tests/test_engine_integration.cpp`

End-to-end integration tests:

1. create processes, run several ticks, verify `NEW -> READY -> RUNNING`
2. verify `Scheduler` updates `runningPID` and `ganttLog`
3. verify `MemoryManager` creates page tables and records page faults
4. verify `SyncManager` blocks/unblocks processes and rebuilds `blockedQueues`
5. verify `TICK_ADVANCED` is emitted once per completed tick
6. verify reset followed by restart works without recreating the controller object

#### [MODIFY] `os-simulator/tests/CMakeLists.txt`

Add both new test executables and link them against:

- `core_lib`
- `process_manager_lib`
- `scheduler_lib`
- `memory_manager_lib`
- `sync_manager_lib`
- `GTest::gtest_main`

---

### Component 9 - Tighten Locking Discipline for Integration Safety

The SDD mentions fine-grained locking by subsystem, but the current codebase is not structured that way yet.

#### Phase 6 locking rule

For Phase 6, use this rule:

- controller owns tick-time write serialization
- worker thread enters module `onTick()` only while holding `std::unique_lock<std::shared_mutex> lock(state.stateMutex)`
- snapshot or observer reads may use `std::shared_lock`
- `EventBus` remains independently protected by its own mutex

#### Why this is acceptable for Phase 6

- correctness matters more than theoretical parallelism at this stage
- NFR-07 requires no engine data races
- Phase 7 depends on a stable engine more than a maximally parallel one

#### What Phase 6 does **not** do

It does not attempt:

- simultaneous module mutation of shared state
- per-substructure lock partitioning
- lock-free event buffering
- snapshot/delta reconciliation between modules

Those are larger architectural changes and should not be smuggled into the integration phase unless explicitly approved.

---

## File Summary

| File | Action | Purpose |
|------|--------|---------|
| `os-simulator/engine/core/ClockController.h` | Modify | Expand public API and add runtime coordination fields |
| `os-simulator/engine/core/ClockController.cpp` | Modify | Implement worker threads, clock thread, barrier phases, STEP/AUTO, reset, shutdown |
| `os-simulator/engine/main.cpp` | Modify | Register all modules, add scheduler, and exercise integrated runtime |
| `os-simulator/tests/test_clock_controller.cpp` | New | Controller lifecycle and mode tests |
| `os-simulator/tests/test_engine_integration.cpp` | New | Cross-module integration tests |
| `os-simulator/tests/CMakeLists.txt` | Modify | Build and register new test targets |

---

## Open Questions

1. Do you want to keep the phased worker-thread model for Phase 6, or do you want a broader refactor toward true concurrent module mutation now?
2. Should `/sim/step` only work while `RUNNING`, or should it also advance while `PAUSED`?
3. Should `main.cpp` remain a human-readable demo harness, or should it become a minimal engine bootstrap intended mainly for testing until Phase 7 arrives?

---

## Deferred Items (For Future Phases)

### 1. True Concurrent Module Mutation

If the team later wants to match the SDD's "all modules run concurrently" model more literally, that should be done as a dedicated refactor:

- compute module-local deltas against a read-only snapshot
- merge deltas in a controlled commit phase
- split `SimulationState` ownership boundaries more clearly

That is outside the safe scope of this Phase 6 integration pass.

### 2. WebSocket Broadcasting and REST Commands

Phase 6 only guarantees the engine-side runtime needed by the bridge. The actual REST endpoints and broadcast server remain Phase 7 work.

### 3. UI-Facing Snapshot Serialization

Phase 6 should preserve correct state and event flow, but JSON serialization and packet shaping belong to the API Bridge phase.

---

## Verification Plan

### Automated Tests

Run the full test suite and require all existing module tests plus the two new integration tests to pass:

```powershell
cmake --build build
ctest --test-dir build --output-on-failure
```

### Key Assertions

- one step request produces exactly one tick
- AUTO mode advances repeatedly until paused
- no tick advances while paused
- `TICK_ADVANCED` count matches completed ticks
- all four modules can participate in the same run
- reset clears process, queue, and memory state
- reset does not remove long-lived event subscriptions
- restart after reset works without recreating the whole runtime

### Manual Verification

1. Run `os_simulator`
2. Confirm all four modules register successfully
3. Start in STEP mode and advance a few ticks
4. Confirm process admission, dispatch, page fault activity, and blocked queue updates appear in output
5. Switch to AUTO mode and verify ticks continue without manual stepping
6. Pause, verify tick count stops increasing
7. Reset, then start again and confirm the engine behaves like a fresh session

---

## Exit Criteria

Phase 6 should be considered done only when all of the following are true:

- `ClockController` is no longer a sequential stub
- worker threads and the clock thread are stable across start/pause/reset/shutdown
- STEP and AUTO mode behavior matches the approved semantics
- `main.cpp` runs all four modules together
- the engine can reset and restart cleanly
- integration tests pass consistently across repeated runs
