# Phase 7 — API Bridge Implementation Plan

## Overview

Phase 7 adds the API Bridge layer (Layer 2 from SDD §5) to the simulation engine. This is a thin C++ HTTP + WebSocket server that sits between the engine and the React frontend. It owns **zero** simulation logic. Its responsibilities:

1. Receive commands from the frontend via **HTTP REST**
2. Forward them to the engine modules
3. Broadcast engine state changes to all connected clients via **WebSocket**

**Technology**: Crow (lightweight C++ HTTP framework) for both REST and WebSocket on a single port (8080). JSON serialisation via nlohmann/json. Both fetched via CMake FetchContent.

> [!IMPORTANT]
> This phase adds **new files only** in `engine/bridge/` and modifies `engine/CMakeLists.txt` and `engine/main.cpp`. No existing module code is modified. All Phase 1–6 tests must continue to pass.

---

## Dependencies

| Dependency | Version | Method | Purpose |
|-----------|---------|--------|---------|
| Crow | v1.2.0 | FetchContent | REST + WebSocket server |
| nlohmann/json | v3.11.3 | FetchContent | JSON serialization/deserialization |

Both are header-only, zero external dependencies.

---

## REST API — Command Endpoints (SDD §5.2)

All endpoints return JSON. All are synchronous.

| Method | Endpoint | Payload | Engine Call |
|--------|----------|---------|------------|
| POST | `/sim/start` | `{}` | `clock.start()` |
| POST | `/sim/pause` | `{}` | `clock.pause()` |
| POST | `/sim/reset` | `{}` | `clock.reset()` |
| POST | `/sim/step` | `{}` | `clock.requestStep()` |
| POST | `/sim/mode` | `{"mode":"STEP\|AUTO"}` | `clock.setMode(simModeFromString(...))` |
| POST | `/sim/speed` | `{"ms": 200}` | `clock.setAutoSpeedMs(...)` |
| POST | `/process/create` | ProcessSpec JSON | `processManager.createProcess(state, bus, spec)` |
| POST | `/process/kill` | `{"pid": N}` | `processManager.killProcess(state, bus, pid)` |
| POST | `/scheduler/policy` | `{"policy":"RR"}` | `scheduler.setPolicy(...)` |
| POST | `/scheduler/quantum` | `{"quantum": 4}` | `clock.setTimeQuantum(...)` |
| POST | `/memory/policy` | `{"policy":"LRU"}` | `memoryManager.setPolicy(...)` |
| POST | `/workload/load` | `{"scenario":"mixed"}` | WorkloadLoader creates ProcessSpecs |
| GET | `/state/snapshot` | — | `serializeState(state)` |

CORS headers enabled for `localhost:5173` (Vite dev server).

---

## WebSocket — State Stream (SDD §5.3)

- Crow native WebSocket route at `/ws` on same port 8080
- On `TICK_ADVANCED` event (subscribed via EventBus), serialize full state and broadcast
- State packet format per SDD §5.3:

```json
{
  "tick": 1024,
  "status": "RUNNING",
  "mode": "AUTO",
  "auto_speed_ms": 200,
  "running_pid": 3,
  "ready_queue": [5, 7, 2],
  "active_policy": "ROUND_ROBIN",
  "time_quantum": 4,
  "active_replacement": "LRU",
  "processes": [{ "pid": 1, "name": "proc_1", "state": "RUNNING", ... }],
  "gantt_log": [{ "tick": 1024, "pid": 3, "policy_snapshot": "ROUND_ROBIN" }],
  "metrics": { "avg_waiting_time": 4.33, ... },
  "frame_table": [{ "frame_number": 0, "occupied": true, ... }],
  "page_tables": { "1": { "owner_pid": 1, ... } },
  "mutex_table": { ... },
  "semaphore_table": { ... },
  "blocked_queues": { ... },
  "mem_metrics": { "total_page_faults": 15, ... },
  "events": [{ "event_type": "CPU_SCHEDULED", ... }],
  "decision_log": [{ "tick": 1024, "message": "..." }]
}
```

JSON field naming: camelCase C++ → snake_case JSON (DataDictionary §9).

---

## Files

### New Files

| File | Responsibility |
|------|----------------|
| `engine/bridge/StateSerializer.h` | `serializeState()` — converts SimulationState to JSON |
| `engine/bridge/WorkloadLoader.h` | Prebuilt scenario loader (cpu_bound, io_bound, mixed) |
| `engine/bridge/RestServer.h` | RestServer class declaration |
| `engine/bridge/RestServer.cpp` | REST endpoints + WebSocket broadcast implementation |

### Modified Files

| File | Change |
|------|--------|
| `engine/CMakeLists.txt` | Add FetchContent for Crow + nlohmann/json, add bridge_lib |
| `engine/main.cpp` | Replace console demo with HTTP server startup |

### Unchanged Files

All files in `core/`, `modules/`, and `tests/` remain untouched.

---

## WorkloadLoader Scenarios (DataDictionary §8.3)

| Scenario | Count | Composition |
|----------|-------|-------------|
| `cpu_bound` | 5 | All CPU_BOUND |
| `io_bound` | 5 | All IO_BOUND |
| `mixed` | 8 | 3 CPU + 3 IO + 2 MIXED |

---

## main.cpp Changes

The current `main.cpp` is a Phase 6 console demo. It will be replaced with:

1. Instantiate SimulationState, EventBus, ClockController
2. Register all 4 modules (same as before)
3. Bootstrap frame table (same as before)
4. Instantiate RestServer with references to all components
5. Call `server.start(8080)` — blocking Crow event loop
6. On SIGINT/SIGTERM: `clock.shutdown()`, exit cleanly

The console demo code is preserved in the git history.

---

## Verification Plan

### Regression
```bash
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```
All Phase 1–6 tests must pass with zero modifications.

### Smoke Test
```powershell
.\build\engine\os_simulator.exe   # starts HTTP server on :8080

# In another terminal:
Invoke-RestMethod -Method POST -Uri http://localhost:8080/sim/start
Invoke-RestMethod -Method POST -Uri http://localhost:8080/sim/step
Invoke-RestMethod -Uri http://localhost:8080/state/snapshot
```
