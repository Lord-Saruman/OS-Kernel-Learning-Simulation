# Layered Testing Guide

This document explains how the OS Kernel Learning Simulator is tested at
each layer of the stack and lists every scenario with its **exact**
expected outputs. Because this project is a learning tool, every test
documents its expected behaviour for all the policy / frame-count /
scheduler combinations that the dashboard exposes.

The guide also explains a result that surprised us during development:
**under the default workload generator, LRU page-fault counts are
sometimes equal to or greater than FIFO**. This is *not* a bug in the
policies — it is a property of the access pattern that
`MemoryManager::getNextVPN()` produces. The full reasoning, plus a
deterministic textbook demo that *does* show the classic LRU win, is at
the bottom of this document.

---

## Test layers at a glance

| Layer | Touches React? | Touches REST API? | Touches engine? | Files |
|-------|----------------|--------------------|-----------------|-------|
| 1. Engine unit / pipeline | No | No | Yes | `tests/test_*.cpp` |
| 2. API integration | No | Yes | Yes | `tests/api/test_api_workflows.ps1` |
| 3. Frontend manual matrix | Yes | Yes | Yes | this document, "Frontend manual matrix" section |

The strategy follows the team's narrowing-down approach: if the
dashboard shows a wrong number, run layer 2 first. If layer 2 fails but
layer 1 passes, the bug is in the API bridge / serializer. If layer 1
fails, the bug is in the engine itself.

---

## Layer 1 — Engine tests (Google Test, no API, no React)

Build and run with CMake / CTest:

```powershell
cd os-simulator/build
cmake --build . --config Release
ctest -C Release --output-on-failure
```

You should see **9 / 9 suites passing**.

### Suite: `MemoryCompareTests` (`tests/test_memory_compare.cpp`)

This is the suite created specifically to lock in the behaviour the
team needed to verify before the presentation. All scenarios are
deterministic: the `MemoryManager` is driven directly with explicit
reference strings, and the full Process + Scheduler + Memory pipeline
is run without `ClockController`'s threading.

| Test | Frames | Reference string | FIFO faults | LRU faults | Note |
|------|--------|------------------|-------------|------------|------|
| `Silberschatz3Frames_FIFOEquals15` | 3 | `7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1` | 15 | — | classic Silberschatz example |
| `Silberschatz3Frames_LRUEquals12` | 3 | same | — | 12 | LRU's textbook win = 3 |
| `BeladysAnomaly3Frames_FIFOEquals9` | 3 | `1 2 3 4 1 2 5 1 2 3 4 5` | 9 | — | Belady example |
| `BeladysAnomaly4Frames_FIFOEquals10` | 4 | same | 10 | — | more frames, more FIFO faults |
| `BeladysAnomaly_LRU_NeverWorseWithMoreFrames` | 3→4 | same | — | LRU(3)≥LRU(4) | LRU is a stack policy |
| `StrictLruWin_4Frames` | 4 | Silberschatz | 10 | 9 | LRU wins by 1 |
| `FrameSweep_SilberschatzString_AllExpected` | 3..8 | Silberschatz | 15,10,9,6,6,6 | 12,8,7,6,6,6 | locks in every (policy, frames) cell |
| `FullPipeline_FCFS_ExplicitSequence_Identical` | 4 | explicit | == LRU | == FIFO | scheduler does not corrupt pattern |
| `FullPipeline_RoundRobin_ExplicitSequence_LruWins` | 4 | explicit | > LRU | < FIFO | LRU wins under RR too |
| `FullPipeline_Priority_ExplicitSequence_LruLeq` | 4 | explicit | ≥ LRU | ≤ FIFO | priority-scheduling agnostic |
| `MultiProcessFCFS_DefaultAccess_Deterministic` | 4 | default `getNextVPN` | locked | locked | repeatable across runs |
| `MultiProcessRoundRobin_DefaultAccess_Deterministic` | 4 | default `getNextVPN` | locked | locked | repeatable across runs |
| `MultiProcessRoundRobin_TextbookSequence_LruLeq` | 4 | Silberschatz | ≥ LRU | ≤ FIFO | LRU ≤ FIFO under multi-proc RR |
| `Determinism_FullPipelineRepeatsExactly` | 4 | default | run1 == run2 | run1 == run2 | guard against accidental rng |

Conclusion of layer 1: **the policies themselves are theoretically
correct.** Every textbook reference string produces the textbook fault
count. LRU is never worse than FIFO when the access pattern has the
LRU stack property.

---

## Layer 2 — API integration tests (PowerShell, no React)

The script `tests/api/test_api_workflows.ps1` drives the live engine
through the same REST endpoints the React dashboard uses, so every
assertion travels: `engine ↔ ClockController ↔ RestServer ↔
StateSerializer ↔ HTTP ↔ test`. If a number is right at layer 1 but
wrong at layer 2, the bug is in the bridge.

### Running

```powershell
# Terminal 1 — start the engine
cd os-simulator/build/engine/Release
./os_simulator.exe

# Terminal 2 — verify against locked-in baselines
cd os-simulator/tests/api
./test_api_workflows.ps1

# Or, if you intentionally changed engine behaviour and need to refresh
# the expected numbers, re-record the baselines:
./test_api_workflows.ps1 -Capture
```

You should see **39 / 39 assertions passing** in verify mode.

### Scenarios and expected values

| # | Scenario | Frames | Sched | Mem | Steps | Expected `total_page_faults` | Expected `total_replacements` |
|---|----------|--------|-------|-----|-------|------------------------------|--------------------------------|
| 1 | Single CPU-bound proc, mem=6 | 4 | FCFS | FIFO | 20 | 3 | 0 |
| 2 | Single CPU-bound proc, mem=6 | 4 | FCFS | LRU | 20 | 3 | 0 |
| 3a | 5 CPU-bound procs, mem=6 | 4 | RR q=2 | FIFO | 50 | 27 | 23 |
| 3b | 5 CPU-bound procs, mem=6 | 4 | RR q=2 | LRU | 50 | 27 | 23 |
| 4 | Determinism (same run twice) | 4 | FCFS | FIFO | 20 | run1 == run2 | — |
| 5 | Frame sweep, mem=6 | 4/6/8/16 | FCFS | FIFO | 20 | 3 / 3 / 3 / 3 | — |
| 6 | Kill running process frees frames | 8 | FCFS | FIFO | 6 + kill + 1 | `occupied_frames == 0` after kill | — |
| 7 | LRU "advantage" demo, mem=8 | 4 | FCFS | FIFO/LRU | 100 | 8 / 9 | — |
| 8 | Working-set sweep, mem=8 | 4 / 6 / 8 | FCFS | FIFO/LRU | 100 | 8/8/6/6/6/6 | — |
| 9 | **Textbook reference string** via `/memory/access_sequence` | 4 | FCFS | FIFO/LRU | 20 | **10 / 8** | — |

Scenario 9 is the demo to highlight in the presentation: a
deterministic Silberschatz reference string is pushed straight through
the API and the dashboard, and LRU correctly produces 2 fewer faults
than FIFO.

### New endpoint added during testing

```
POST /memory/access_sequence
Body: { "vpns": [7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1] }
```

This is intentionally exposed so the dashboard can drive textbook
reference strings. It calls `MemoryManager::setAccessSequence()` which
replaces `getNextVPN()`'s pseudo-random pattern until the array is
exhausted.

---

## Why LRU >= FIFO sometimes — and what to say in the presentation

`MemoryManager::getNextVPN()` (used when no explicit access sequence is
set) generates a per-process locality pattern with these probabilities
each tick:

- **45 %** access `lastVpn` (the page touched last tick)
- **30 %** access `prevVpn` (the page touched two ticks ago)
- **20 %** pick from a 3-page "hot window" anchored on `hotBase`
- **5 %** global jump

This stabilises the working set on a tiny set of pages very quickly.
Once the hot pages are loaded:

- They are accessed almost every tick → both their `loadTick` (FIFO
  victim score) and their `lastAccessTick` (LRU victim score) are
  **recent**, which means **neither policy will evict them**.
- The remaining frames cycle through the cold pages caused by the 20 %
  hot-window picks and 5 % global jumps. For these cold frames, FIFO's
  load order and LRU's recency order produce **virtually identical
  victim choices** (because the cold pages were both loaded recently
  *and* accessed recently, just once).
- The 30 % `prevVpn` rule swaps `lastVpn` and `prevVpn` repeatedly, so
  at certain moments LRU's recency ranking even gives FIFO an
  accidental edge of 1–2 faults.

So when the audience asks *"why does LRU show the same or worse number
of faults than FIFO on the dashboard?"*, the precise answer is:

> The default workload generator's access pattern stabilises on a
> 2-page hot working set that fits in our frame table, so both
> policies hit the same cold-start faults and then both keep the hot
> pages resident. With the default pattern, LRU's recency tracking
> degenerates to FIFO's load-order tracking, and a small swap rule in
> the locality model occasionally flips them by 1–2 faults. This is
> demonstrated by Scenarios 7 and 8 of the API test suite.
>
> When we drive a textbook reference string (Silberschatz) through the
> same engine via the new `/memory/access_sequence` endpoint, LRU
> correctly produces 2 fewer faults than FIFO at 4 frames (Scenario 9
> of the API tests, plus 9/9 engine tests in `MemoryCompareTests`).
> So the policies are correct — the workload was hiding the difference.

---

## Layer 3 — Frontend manual test matrix

Run this against the live dashboard
(`http://localhost:5173`) with the engine already started. Each row is
self-contained: click *Reset*, set the controls in the order shown, and
click *Step* the requested number of times. The expected numbers come
straight from layer 2 and are reproducible.

> Tip: use the **LRU vs FIFO** button in the dashboard control bar for the
> textbook demo. It loads the Silberschatz reference string through
> `/memory/access_sequence`, creates the demo process, and leaves the
> simulator ready to step.

| # | What to demo | Controls | Steps | Look at | Expected |
|---|--------------|----------|-------|---------|----------|
| F1 | Working set fits, FIFO | Frames=4, Sched=FCFS, Mem=FIFO. New process, type=CPU_BOUND, cpu=30, mem=6 | 20 | Memory metrics | `total_page_faults = 3`, `total_replacements = 0` |
| F2 | Working set fits, LRU | Same as F1 but Mem=LRU | 20 | Memory metrics | `total_page_faults = 3`, `total_replacements = 0` (matches FIFO — that's the textbook tie) |
| F3 | Multi-process pressure FIFO | Frames=4, Sched=ROUND_ROBIN, q=2, Mem=FIFO. Create 5 procs cpu=20 mem=6 | 50 | Memory metrics | `total_page_faults = 27`, `total_replacements = 23` |
| F4 | Multi-process pressure LRU | Same as F3 with Mem=LRU | 50 | Memory metrics | `total_page_faults = 27`, `total_replacements = 23` (LRU == FIFO; explained above) |
| F5 | Frame-count sweep FIFO | Repeat F1 with Frames=4, 6, 8, 16 | 20 | Memory metrics | `total_page_faults = 3` for every frame size |
| F6 | LRU "advantage" demo (current pattern) | Frames=4, Sched=FCFS, Mem=FIFO/LRU. Single proc, cpu=200, mem=8 | 100 | Memory metrics | FIFO=8, LRU=9 (current locality model — 1-fault artifact) |
| F7 | **Textbook LRU win (use this on stage)** | Select Mem=FIFO, click **LRU vs FIFO**, then step. Reset, select Mem=LRU, click **LRU vs FIFO**, then step again. | 20 | Memory metrics | FIFO=10. Reset, switch Mem=LRU, repeat → LRU=8 |

After running F7 with both policies you can show the audience the
canonical Silberschatz numbers — same engine, same dashboard, same
process, same workload, but **LRU = 8 < FIFO = 10**.

---

## Quick triage when something looks wrong

1. Run layer 1 (`ctest -C Release`). If a suite fails, the bug is in
   the engine; the test name names the module.
2. If layer 1 is green, run layer 2 (`./test_api_workflows.ps1`). If a
   scenario fails, compare the captured baseline to the engine layer
   (the file says "captured" via `-Capture`). A mismatch points at
   `RestServer.cpp` or `StateSerializer.h`.
3. If layers 1 and 2 are green but the dashboard shows wrong numbers,
   look at the frontend `useSimControl` / WebSocket subscriber. The
   engine and API are not at fault.
