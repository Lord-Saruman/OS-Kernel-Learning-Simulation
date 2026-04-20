
MINI OS KERNEL SIMULATOR
System Design Document (SDD)


Document
System Design Document — v1.0
Project
Mini OS Kernel Simulator
Course
CS-330 Operating Systems — BESE 30
Status
DRAFT
Date
April 2026

Companion Document to: Product Requirements Document v1.0

1. Introduction
1.1 Purpose
This System Design Document (SDD) defines the complete technical architecture of the Mini OS Kernel Simulator. It is the authoritative reference that all team members use before writing any code. It defines how the system is structured, how its components communicate, which technology implements each responsibility, and what design decisions were made and why.

Every module boundary, interface contract, and data flow documented here must remain stable throughout the project. Any change to this document requires team consensus.
1.2 Scope
This document covers the entire system: the C++ simulation engine and its four OS subsystem modules, the API bridge layer, the React dashboard, and the communication protocols between them. It does not cover detailed data structure field definitions (Data Dictionary) or function-level pseudocode (LLD), which are addressed in separate documents.
1.3 Design Principles
The following principles govern every architectural decision in this document:

Principle
What It Means
Hard Module Boundaries
No subsystem module may import or call another subsystem module directly. All cross-module communication goes through the shared Simulation State or Event Bus only.
Single Source of Truth
The C++ Simulation State object is the only authoritative source of system state. The frontend is a read-only view; it never holds its own state independently.
Policy = Plugin
Scheduling algorithms and page replacement policies are implemented as swappable strategy objects. Adding a new policy requires zero changes to the module that uses it.
Visibility by Default
Every state transition and scheduling decision is logged to an observable event stream. Nothing happens silently inside the engine.
Engine Knows Nothing of UI
The C++ engine has no knowledge of the React frontend. It exposes an API and emits state. What consumes that is irrelevant to the engine.

2. High-Level System Architecture
2.1 Three-Layer Architecture
The system is organised into three distinct, independently deployable layers. Each layer has a single, well-defined responsibility and communicates with adjacent layers only through defined contracts.

┌─────────────────────────────────────────────────────┐
│           LAYER 3 — PRESENTATION LAYER              │
│              React Dashboard (Browser)               │
│  Process View │ Scheduler View │ Sync View │ Mem View │
└──────────────────────┬──────────────────────────────┘
                       │  REST (commands) + WebSocket (state stream)
┌──────────────────────┴──────────────────────────────┐
│           LAYER 2 — API BRIDGE LAYER                 │
│   HTTP REST Server  +  WebSocket State Broadcaster   │
└──────────────────────┬──────────────────────────────┘
                       │  Direct C++ function calls
┌──────────────────────┴──────────────────────────────┐
│           LAYER 1 — SIMULATION ENGINE LAYER          │
│  ┌─────────────┐ ┌──────────┐ ┌───────┐ ┌────────┐  │
│  │Process Mgr  │ │Scheduler │ │Sync   │ │Memory  │  │
│  └──────┬──────┘ └────┬─────┘ └───┬───┘ └───┬────┘  │
│         └─────────────┴───────────┴───────────┘       │
│            Simulation State  +  Event Bus              │
└─────────────────────────────────────────────────────┘

2.2 Layer Responsibilities Summary
Layer
Technology
Single Responsibility
Layer 1 — Engine
C++ 17, std::thread
Owns and mutates all simulation state. Runs the OS logic. Has no knowledge of any consumer.
Layer 2 — Bridge
C++ (crow / httplib + websocketpp)
Translates HTTP/WebSocket protocol into engine function calls. Serialises state to JSON. Owns no logic.
Layer 3 — Dashboard
React + TypeScript
Renders state received from the bridge. Sends user commands as REST calls. Owns zero simulation state.

3. Simulation Engine Architecture (Layer 1)
3.1 Internal Structure
The simulation engine is a single C++ application composed of six components. Four of these are the OS subsystem modules. The other two are the Simulation State and the Event Bus — the backbone that keeps modules decoupled.

Component
Type
Responsibility
Simulation State
Shared Object
Central data store. Holds all PCBs, TCBs, page tables, ready queues, frame tables, semaphore states, and clock tick counter. Protected by a global read-write lock.
Event Bus
Pub/Sub
Decoupled messaging. Modules publish events (e.g. PROCESS_STATE_CHANGED). The API bridge subscribes and streams to the frontend. Modules never call each other directly.
Process Manager Module
OS Module
Creates/terminates processes and threads. Manages PCB and TCB lifecycle. Drives state machine transitions. Publishes state change events.
Scheduler Module
OS Module
Owns the ready queue. Selects next process per active policy. Manages time quantum (RR). Records scheduling metrics. Policies are injected as strategy objects.
Sync Manager Module
OS Module
Manages mutexes and semaphores. Blocks and unblocks processes. Maintains per-primitive blocked queues. Publishes lock/unlock events.
Memory Manager Module
OS Module
Manages frame table and per-process page tables. Handles page faults. Executes active replacement algorithm. Publishes fault events.

3.2 The Simulation Clock
The engine operates on a discrete clock tick model. A central Clock Controller thread advances a global tick counter. Each tick represents one unit of simulated CPU time. On each tick, modules execute in the following deterministic order:
The Process Manager admits new processes (NEW → READY) and completes I/O waits for waiting processes
The Scheduler selects the next running process from the ready queue and decrements its remaining burst
The Memory Manager services any pending page fault for the running process
The Sync Manager checks for lock acquisition by blocked processes
All state changes are published to the Event Bus
The API bridge broadcasts the full updated state snapshot to all WebSocket clients

This order is enforced by the ClockController's phased worker-thread model: each module runs in its own real thread, but only one module executes per phase within a tick, coordinated via std::barrier. This guarantees deterministic, race-free execution while still using real OS threads (NFR-07).

In Step Mode, the Clock Controller waits for an explicit advance signal from the API bridge (or while PAUSED, a single step) before incrementing the tick. In Auto Mode, it advances on a configurable timer interval.

3.3 Module Interface Contract
Every OS module must implement the following interface. This is enforced via a C++ abstract base class (ISimModule). Any new module added in the future must conform to this same interface — this is what makes the system extensible.

class ISimModule {
  public:
    virtual void onTick(SimulationState& state, EventBus& bus) = 0;
    virtual void reset()                                        = 0;
    virtual ModuleStatus getStatus() const                      = 0;
    virtual std::string  getModuleName() const                  = 0;
    virtual ~ISimModule() = default;
};

The onTick() method is called once per clock tick. All module logic lives inside this method. Modules read from and write to the shared SimulationState object. They never call other modules.

3.4 Strategy Pattern for Policies
CPU scheduling algorithms and page replacement policies are implemented as strategy objects, not hardcoded logic. The Scheduler module holds a pointer to the active ISchedulingPolicy. The Memory Manager holds a pointer to the active IReplacementPolicy. Swapping a policy at runtime is a single pointer reassignment — zero changes to module logic.

// Scheduling Strategy Interface
class ISchedulingPolicy {
  public:
    virtual PCB* selectNext(ReadyQueue& queue) = 0;
    virtual std::string policyName() const     = 0;
};

// Concrete Implementations
class FCFSPolicy    : public ISchedulingPolicy { ... };
class RoundRobin    : public ISchedulingPolicy { ... };
class PriorityPolicy: public ISchedulingPolicy { ... };

// Page Replacement Strategy Interface
class IReplacementPolicy {
  public:
    virtual int selectVictimFrame(FrameTable& frames) = 0;
    virtual std::string policyName() const            = 0;
};

class FIFOPolicy : public IReplacementPolicy { ... };
class LRUPolicy  : public IReplacementPolicy { ... };

4. Event Bus and Simulation State
4.1 Event Bus Design
The Event Bus is a synchronous publish-subscribe mechanism implemented as a singleton within the engine. It is the only communication channel between modules and the outside world. Modules never know who (if anyone) is listening to their events.

Event Type
Published By → Consumed By
PROCESS_CREATED
Process Manager → API Bridge, Memory Manager
PROCESS_STATE_CHANGED
Process Manager → API Bridge, Scheduler
PROCESS_TERMINATED
Process Manager → API Bridge, Scheduler, Memory Manager
CPU_SCHEDULED
Scheduler → API Bridge
CONTEXT_SWITCH
Scheduler → API Bridge
LOCK_ACQUIRED
Sync Manager → API Bridge
LOCK_RELEASED
Sync Manager → API Bridge
PROCESS_BLOCKED
Sync Manager → API Bridge, Scheduler
PROCESS_UNBLOCKED
Sync Manager → API Bridge, Scheduler
PAGE_FAULT
Memory Manager → API Bridge
PAGE_REPLACED
Memory Manager → API Bridge
TICK_ADVANCED
Clock Controller → API Bridge, all modules

4.2 Simulation State Structure (Top-Level)
The SimulationState object is the single source of truth. It is a C++ struct held in heap memory, accessed by all modules via reference. All mutations are protected by a std::shared_mutex (multiple readers, one writer at a time).

struct SimulationState {
  // Clock
  uint64_t                      currentTick;
  SimMode                       mode;         // STEP | AUTO
  SimStatus                     status;       // IDLE | RUNNING | PAUSED
  uint32_t                      autoSpeedMs;  // tick interval in ms

  // Process Manager
  std::map<int, PCB>            processTable;
  std::map<int, TCB>            threadTable;
  int                           nextPID;
  int                           nextTID;

  // Scheduler
  std::deque<int>               readyQueue;   // PIDs
  int                           runningPID;   // -1 if idle
  std::string                   activePolicy; // "FCFS" | "RR" | "PRIORITY"
  uint32_t                      timeQuantum;  // for RR
  std::vector<GanttEntry>       ganttLog;
  SchedulingMetrics             metrics;

  // Sync Manager
  std::map<int, Mutex>          mutexTable;
  std::map<int, Semaphore>      semaphoreTable;
  std::map<int, std::deque<int>> blockedQueues; // syncID -> blocked PIDs

  // Memory Manager
  FrameTable                    frameTable;
  std::map<int, PageTable>      pageTables;   // PID -> PageTable
  std::string                   activeReplacement; // "FIFO" | "LRU"
  MemoryMetrics                 memMetrics;

  // Decision Log (for UI annotation)
  std::vector<DecisionLogEntry> decisionLog;
};

5. API Bridge Layer (Layer 2)
5.1 Role and Boundaries
The API Bridge is a thin C++ server process that sits between the simulation engine and the React frontend. It owns no simulation logic. Its only responsibilities are: receiving commands from the frontend via HTTP REST, forwarding them to the engine, and broadcasting engine state changes to the frontend via WebSocket.

Technology recommendation: Crow (lightweight C++ HTTP framework) for REST endpoints, with a WebSocket server running on a separate port. Both can be embedded in the same process as the engine.

5.2 REST API — Command Endpoints
The following endpoints allow the frontend to control the simulation. All return JSON. All are synchronous — they execute immediately and return the updated top-level status.

Method
Endpoint
Payload
Action
POST
/sim/start
{}
Start or resume the simulation clock
POST
/sim/pause
{}
Pause the clock without resetting state
POST
/sim/reset
{}
Stop and clear all simulation state
POST
/sim/step
{}
Advance exactly one clock tick (Step Mode only)
POST
/sim/mode
{mode: STEP|AUTO}
Switch between Step and Auto simulation mode
POST
/sim/speed
{ms: 200}
Set Auto Mode tick interval in milliseconds
POST
/process/create
ProcessSpec JSON
Inject a new process into the simulation
POST
/process/kill
{pid: N}
Force-terminate a running process
POST
/scheduler/policy
{policy: "RR"}
Swap the active scheduling algorithm live
POST
/scheduler/quantum
{quantum: 4}
Update RR time quantum
POST
/memory/policy
{policy:"LRU"}
Swap the active page replacement algorithm live
POST
/workload/load
{scenario: "mixed"}
Load a prebuilt workload (cpu | io | mixed)
GET
/state/snapshot
—
Get full current simulation state as JSON

5.3 WebSocket — State Stream
The WebSocket server runs continuously and broadcasts a state update packet to all connected clients after every clock tick. Clients do not need to poll — they subscribe once and receive a live stream of state changes.

// State Update Packet (sent after every tick)
{
  "tick":          1024,
  "status":        "RUNNING",
  "runningPID":    3,
  "readyQueue":    [5, 7, 2],
  "processes":     [ { pid, name, state, priority, remainingBurst, ... } ],
  "ganttEntry":    { pid: 3, tick: 1024 },
  "metrics":       { avgWait, avgTurnaround, cpuUtil },
  "memMetrics":    { pageFaults, faultRate },
  "events":        [ { type, description, tick } ],
  "decisionLog":   [ { tick, message } ]
}

6. React Dashboard Architecture (Layer 3)
6.1 Frontend Design Principles
The frontend holds zero simulation state. All state lives in the engine.
On mount, the React app connects to the WebSocket and receives a full state snapshot.
Every subsequent tick delivers a state update packet which replaces the local display state.
User actions call REST endpoints — they do not mutate local state directly.

6.2 Component Tree

Component
Responsibility
App (Root)
WebSocket connection manager. Holds the single display state object. Distributes state slices to child panels.
SimControlBar
Start / Pause / Reset / Step / Speed slider / Mode toggle. Sends REST commands on user action.
ProcessPanel
Displays all processes as animated state cards. Visualises the process state machine. Shows PCB details on hover.
SchedulerPanel
Ready queue visualisation. Live Gantt chart. Scheduling metrics table. Algorithm selector dropdown.
SyncPanel
Mutex and semaphore status. Blocked process queues per lock. Race condition demonstration toggle.
MemoryPanel
Frame table visualisation. Per-process page tables. Page fault counter and fault rate chart. Policy selector.
DecisionLogPanel
Scrollable real-time log of every scheduling and memory decision with plain-English annotation.
MetricsPanel
Side-by-side policy comparison view. Line/bar charts for waiting time, turnaround, fault rate.
ConceptTooltip
Reusable overlay component attached to each panel header. Shows plain-English explanation of the OS concept.
WorkloadLauncher
Pre-built scenario loader. Lets user pick CPU-bound, I/O-bound, or mixed and fires /workload/load.

6.3 State Management
The frontend uses React's built-in useState and useReducer. No external state library (Redux, Zustand) is required because the state is simple: it is exactly the last WebSocket packet received. The WebSocket handler calls a single dispatch(action) that replaces the entire display state on every tick.

// Single display state — replaced wholesale on every WebSocket tick
const [simState, dispatch] = useReducer(simReducer, initialState);

useEffect(() => {
  const ws = new WebSocket('ws://localhost:8081');
  ws.onmessage = (event) => {
    dispatch({ type: 'STATE_UPDATE', payload: JSON.parse(event.data) });
  };
  return () => ws.close();
}, []);

7. Concurrency Model
7.1 Thread Inventory
The C++ engine uses real OS threads. The following threads exist at runtime:

Thread
Count
Role
Clock Controller
1
Drives tick advancement. In Auto Mode sleeps for the configured interval then fires a tick. In Step Mode blocks on a condition variable until signalled.
Module Worker Threads
1 per module (4 total)
Each OS module runs its onTick() logic in its own thread, triggered by a tick signal from the Clock Controller via a barrier synchronisation.
HTTP Server Thread
1
Handles incoming REST requests. Acquires write lock on SimulationState when executing commands that mutate state.
WebSocket Broadcast Thread
1
After all module threads complete their onTick(), serialises the SimulationState to JSON and broadcasts to all connected clients.

7.2 Tick Synchronisation Sequence
Each clock tick follows a strict sequence to prevent race conditions within the engine itself:

Step
Action
1
Clock Controller increments tick counter and signals the first module thread.
2
Module threads execute their onTick() method in deterministic order (Process → Scheduler → Memory → Sync), each acquiring an exclusive write lock on SimulationState during its phase. Only one module mutates state at a time.
3
All four phases complete — the controller signals tick completion.
4
WebSocket Broadcast Thread acquires a full read lock on SimulationState, serialises it, and broadcasts.
5
Read lock released. HTTP server may now service any pending command requests.
6
Clock Controller waits for interval (Auto) or step signal (Step), then repeats from Step 1.

7.3 Locking Strategy
SimulationState uses a std::shared_mutex — multiple readers, exclusive writer
During tick execution, each module phase acquires an exclusive write lock on the full SimulationState. Only one module runs per phase, so no fine-grained per-subsection locking is needed at this stage.
The Event Bus uses a separate std::mutex for its subscription list and tick event buffer. The EventBus mutex is independent of stateMutex (no nesting constraint).
The HTTP server uses a condition variable to notify the Clock Controller when a step command arrives in Step Mode

8. Technology Stack

Component
Technology
Version
Rationale
Simulation Engine
C++17
C++17
std::thread, std::barrier, std::shared_mutex all available. Closest to real OS implementation.
HTTP REST Server
Crow (C++ framework)
1.x
Header-only, zero dependencies, embeds directly in engine process.
WebSocket Server
websocketpp
0.8.x
Mature, well-documented, integrates with Crow's Asio backend.
JSON Serialisation
nlohmann/json
3.x
Header-only, intuitive API, de-facto standard for C++ JSON.
Frontend Framework
React + TypeScript
React 18
Component model maps cleanly to OS subsystem panels. TypeScript prevents state shape bugs.
Frontend Build
Vite
5.x
Fast dev server with HMR. Trivial to build and serve.
Charts / Visualisation
Recharts
2.x
React-native, declarative, sufficient for Gantt, line, and bar charts.
Build System
CMake
3.20+
Industry standard for C++. Manages module compilation and linking.
Version Control
Git + GitHub
—
One branch per module enables parallel development without conflicts.

9. Deployment Architecture
9.1 Development Setup
All three layers run on a single developer machine during development. This is the only deployment target for this semester project.

Process
Port
Command
C++ Engine + API Bridge
REST: 8080, WS: 8081
./build/os_simulator
React Dashboard
3000
npm run dev (Vite dev server)

The React app connects to localhost:8080 for REST and localhost:8081 for WebSocket. CORS is enabled on the C++ server for localhost origins during development.

9.2 Repository Structure

os-simulator/
├── engine/                    # C++ Simulation Engine
│   ├── core/
│   │   ├── SimulationState.h
│   │   ├── EventBus.h
│   │   └── ClockController.cpp
│   ├── modules/
│   │   ├── process/           # Process Manager Module
│   │   ├── scheduler/         # Scheduler Module + policies/
│   │   ├── sync/              # Sync Manager Module
│   │   └── memory/            # Memory Manager Module + policies/
│   ├── bridge/                # API Bridge (HTTP + WebSocket)
│   │   ├── RestServer.cpp
│   │   └── WsServer.cpp
│   └── CMakeLists.txt
├── dashboard/                 # React Frontend
│   ├── src/
│   │   ├── components/
│   │   │   ├── ProcessPanel/
│   │   │   ├── SchedulerPanel/
│   │   │   ├── SyncPanel/
│   │   │   └── MemoryPanel/
│   │   ├── hooks/
│   │   │   ├── useSimState.ts
│   │   │   └── useSimControl.ts
│   │   └── App.tsx
│   └── package.json
├── tests/                     # Per-module test suites
└── docs/                      # All project documents (PRD, SDD, etc.)

10. Key Design Decisions and Rationale

Decision
Chosen Approach
Alternatives Rejected
Module communication
Shared SimulationState + Event Bus only
Direct module-to-module function calls — creates tight coupling, breaks independent testability
Policy extensibility
Strategy pattern (interface + concrete class)
Enum switch-case inside module — requires touching module code to add new algorithm
Frontend state
Stateless React — WebSocket as source of truth
Redux store mirroring engine state — unnecessary duplication and sync complexity
Real vs simulated threads
Real std::thread with controlled tick barrier
Pure event-loop simulation — less authentic, harder to demonstrate true race conditions
C++ HTTP server
Crow (embedded in engine process)
Separate Python/Node bridge process — extra process management, latency, deployment complexity
UI framework
React + TypeScript + Recharts
Vue or Angular — team familiarity and ecosystem size favour React
