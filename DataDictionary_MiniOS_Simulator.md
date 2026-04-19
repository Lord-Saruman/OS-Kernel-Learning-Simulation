
MINI OS KERNEL SIMULATOR
Data Dictionary


Document
Data Dictionary — v1.0
Project
Mini OS Kernel Simulator
Depends On
PRD v1.0, SDD v1.0
Status
DRAFT

Every field of every data structure used in the simulation engine is defined in this document.

1. Purpose and Usage Rules
This Data Dictionary is the single authoritative reference for every data structure, enum, and field used inside the Mini OS Kernel Simulator engine. Before any team member writes a struct, class, or variable that will be shared between modules or serialised to JSON, they must look it up here first.

Three rules apply to this document:
Rule 1 — No struct may be defined in code that is not first defined here. Implementation must match this document exactly in field names and types.
Rule 2 — If implementation requires a field not listed here, the team must update this document before adding it to code. No undocumented fields.
Rule 3 — The JSON keys used in the WebSocket state packet and REST payloads are the snake_case versions of the C++ field names listed here, unless explicitly noted otherwise.

Column definitions for all field tables:
Column
Meaning
Field Name
Exact name as it appears in C++ struct definition and JSON serialisation
C++ Type
The C++ primitive or class type. Enums reference the enum tables in this document.
Size
Memory footprint in bytes for primitives. 'var' for variable-length containers.
R/W
R = read-only after initialisation. W = mutable during simulation. RW = both.
Description
What this field stores and how it is used by the simulation.
Constraints
Valid value ranges, invariants, or initialisation requirements.

2. Enumeration Types
All enums are defined in engine/core/SimEnums.h and shared across all modules.

2.1 ProcessState
Represents the lifecycle state of a process. A process may only occupy one state at a time. Valid transitions are defined in the Process Manager LLD.

Enum Value
Meaning
NEW
Process has been created but not yet admitted to the ready queue. PCB exists, resources not yet allocated.
READY
Process is in the ready queue waiting for CPU allocation. All resources available.
RUNNING
Process is currently executing on the simulated CPU. Only one process may be RUNNING at any tick.
WAITING
Process is blocked — either waiting for I/O completion or blocked on a mutex/semaphore.
TERMINATED
Process has completed execution or been killed. PCB is retained for metrics collection then removed.

2.2 ThreadState
Thread lifecycle states. A thread inherits WAITING if its parent process enters WAITING.

Enum Value
Meaning
T_NEW
Thread created but not yet scheduled.
T_RUNNABLE
Thread is ready to be scheduled within its process.
T_RUNNING
Thread is currently executing (its parent process is RUNNING).
T_BLOCKED
Thread is blocked on a synchronisation primitive.
T_TERMINATED
Thread has exited. TCB retained briefly then removed from threadTable.

2.3 SimMode

Enum Value
Meaning
STEP
Simulation advances one clock tick at a time. Waits for explicit /sim/step REST call before each tick.
AUTO
Simulation advances automatically on a timer. Interval controlled by autoSpeedMs in SimulationState.

2.4 SimStatus

Enum Value
Meaning
IDLE
Simulation has not been started or has been reset. No clock activity.
RUNNING
Simulation clock is actively advancing ticks.
PAUSED
Clock is suspended. State is preserved. Resumes on /sim/start.
STOPPED
Simulation ended. All state cleared on next /sim/reset.

2.5 SchedulingPolicy

Enum Value
Meaning
FCFS
First-Come-First-Served. Non-preemptive. Ordered by arrivalTick.
ROUND_ROBIN
Round Robin. Preemptive. Uses timeQuantum field from SimulationState.
PRIORITY_NP
Priority Scheduling, Non-Preemptive. Lower priority value = higher priority.
PRIORITY_P
Priority Scheduling, Preemptive. A higher-priority arrival preempts the running process.

2.6 ReplacementPolicy

Enum Value
Meaning
FIFO
First-In-First-Out. Evicts the frame whose page was loaded earliest (by loadTick).
LRU
Least Recently Used. Evicts the frame whose page was accessed least recently (by lastAccessTick).

2.7 ProcessType
Classifies the workload profile of a process. Used to drive workload scenario generation.

Enum Value
Meaning
CPU_BOUND
High CPU burst time, minimal or no I/O bursts. Exercises the scheduler heavily.
IO_BOUND
Short CPU bursts followed by long I/O waits. Process spends most time in WAITING state.
MIXED
Alternating CPU and I/O bursts of moderate length. Represents typical interactive workload.

2.8 SyncPrimitiveType

Enum Value
Meaning
MUTEX
Binary mutual exclusion lock. Owned by exactly one process at a time.
SEMAPHORE_BINARY
Counting semaphore initialised to 1. Equivalent to mutex but without ownership tracking.
SEMAPHORE_COUNTING
Counting semaphore initialised to N > 1. Allows up to N concurrent holders.

3. Process Control Block (PCB)
Defined in: engine/modules/process/PCB.h   |   Owned by: Process Manager   |   Stored in: SimulationState::processTable
The PCB is the central data structure of the simulator. It stores all metadata the OS needs to manage a process throughout its lifecycle. Every process in the simulation has exactly one PCB. The PCB is created in NEW state and removed after TERMINATED state metrics are collected.

struct PCB {
  // Identity
  int           pid;
  std::string   name;
  ProcessType   type;
  int           priority;

  // State
  ProcessState  state;
  uint64_t      arrivalTick;
  uint64_t      startTick;
  uint64_t      terminationTick;

  // CPU Burst
  uint32_t      totalCpuBurst;
  uint32_t      remainingBurst;
  uint32_t      quantumUsed;

  // I/O
  uint32_t      ioBurstDuration;
  uint32_t      ioRemainingTicks;
  uint32_t      ioCompletionTick;

  // Memory
  uint32_t      memoryRequirement;
  int           pageTableId;

  // Metrics (accumulated)
  uint64_t      waitingTime;
  uint64_t      turnaroundTime;
  uint32_t      contextSwitches;
  uint32_t      pageFaultCount;

  // Thread list
  std::vector<int> threadIds;
};

Field Name
C++ Type
Size
R/W
Description
Constraints
pid
int
4
R
Unique process identifier. Auto-assigned by Process Manager from nextPID counter.
pid >= 1. Never reused within a session.
name
std::string
var
R
Human-readable process name shown in the UI. Auto-generated (e.g. 'proc_3') or user-supplied.
Max 32 characters. Non-empty.
type
ProcessType
4
R
Workload classification — CPU_BOUND, IO_BOUND, or MIXED. Determines burst profile.
Must be a valid ProcessType enum value.
priority
int
4
RW
Scheduling priority. Lower integer = higher priority. Used by PRIORITY_NP and PRIORITY_P policies.
Range: 1 (highest) to 10 (lowest). Default: 5.
state
ProcessState
4
W
Current lifecycle state of the process. Modified only by Process Manager on valid transitions.
Must be a valid ProcessState enum value.
arrivalTick
uint64_t
8
R
The clock tick at which the process entered the NEW state. Set on creation, never modified.
arrivalTick >= 0.
startTick
uint64_t
8
W
The clock tick when the process first entered RUNNING state. 0 until first scheduled.
0 until first dispatch. startTick >= arrivalTick.
terminationTick
uint64_t
8
W
The tick at which the process reached TERMINATED state. 0 until terminated.
0 until terminated. >= startTick.
totalCpuBurst
uint32_t
4
R
Total CPU execution time required by this process, in clock ticks.
totalCpuBurst >= 1.
remainingBurst
uint32_t
4
W
CPU ticks remaining until process completes. Decremented each tick the process is RUNNING.
0 <= remainingBurst <= totalCpuBurst. 0 triggers termination.
quantumUsed
uint32_t
4
W
CPU ticks consumed in the current Round Robin quantum. Reset to 0 on each new dispatch.
0 <= quantumUsed <= timeQuantum (SimulationState).
ioBurstDuration
uint32_t
4
R
Duration of a single I/O burst in ticks. 0 for CPU_BOUND processes.
>= 0. 0 means no I/O.
ioRemainingTicks
uint32_t
4
W
Ticks remaining in the current I/O burst. Decremented each tick the process is WAITING for I/O.
0 when not in I/O wait.
ioCompletionTick
uint32_t
4
W
The tick at which the current I/O operation will complete. Set when entering WAITING for I/O.
0 if not currently waiting on I/O.
memoryRequirement
uint32_t
4
R
Number of virtual pages this process requires. Drives page table size on creation.
>= 1. Must not exceed total configured virtual pages.
pageTableId
int
4
R
Key into SimulationState::pageTables. Points to this process's PageTable object.
Matches pid. -1 if not yet allocated (NEW state only).
waitingTime
uint64_t
8
W
Accumulated ticks spent in READY state (waiting for CPU). Updated each tick process is in READY.
Finalised on TERMINATED. Used in scheduling metrics.
turnaroundTime
uint64_t
8
W
terminationTick - arrivalTick. Computed when process reaches TERMINATED state.
0 until terminated.
contextSwitches
uint32_t
4
W
Number of times this process was preempted or descheduled. Incremented on each context switch out.
>= 0.
pageFaultCount
uint32_t
4
W
Number of page faults generated by this process during its lifetime.
>= 0.
threadIds
vector<int>
var
W
List of TIDs belonging to this process. Populated when threads are created.
May be empty if no explicit threads created.

4. Thread Control Block (TCB)
Defined in: engine/modules/process/TCB.h   |   Owned by: Process Manager   |   Stored in: SimulationState::threadTable
Each thread within a process has a TCB. The TCB is lighter than a PCB — it tracks execution context within the process rather than resource ownership. Threads share the memory space (and page table) of their parent process.

struct TCB {
  int           tid;
  int           parentPid;
  ThreadState   state;
  uint64_t      creationTick;
  uint32_t      stackSize;
  uint64_t      simulatedSP;
  uint32_t      cpuBurst;
  uint32_t      remainingBurst;
  int           blockedOnSyncId;
  uint64_t      waitingTime;
};

Field Name
C++ Type
Size
R/W
Description
Constraints
tid
int
4
R
Unique thread identifier. Auto-assigned by Process Manager from nextTID counter.
tid >= 1. Never reused within a session.
parentPid
int
4
R
PID of the process this thread belongs to. Set on creation, immutable.
Must be a valid, non-TERMINATED pid in processTable.
state
ThreadState
4
W
Current thread lifecycle state. Transitions mirror parent process state in most cases.
Must be a valid ThreadState enum value.
creationTick
uint64_t
8
R
The clock tick at which this thread was created. Set on creation.
>= 0.
stackSize
uint32_t
4
R
Simulated stack size in pages. Fixed at thread creation. Not dynamically resizable.
>= 1. Default: 2 pages.
simulatedSP
uint64_t
8
W
Simulated stack pointer — a counter representing stack usage depth. Used in visualisation only.
0 <= simulatedSP <= stackSize * pageSize.
cpuBurst
uint32_t
4
R
Total CPU ticks this thread needs to execute.
>= 1.
remainingBurst
uint32_t
4
W
Remaining CPU ticks for this thread. Decremented when thread is T_RUNNING.
0 <= remainingBurst <= cpuBurst.
blockedOnSyncId
int
4
W
ID of the mutex or semaphore this thread is currently blocked on. -1 if not blocked.
-1 or valid key in mutexTable / semaphoreTable.
waitingTime
uint64_t
8
W
Accumulated ticks this thread spent in T_BLOCKED state waiting on a sync primitive.
>= 0.

5. Scheduling Data Structures
5.1 GanttEntry
Defined in: engine/modules/scheduler/GanttEntry.h   |   Appended to: SimulationState::ganttLog each tick
One GanttEntry is appended to the ganttLog every clock tick to record which process (or idle) held the CPU. The frontend uses this log to render the Gantt chart.

struct GanttEntry {
  uint64_t  tick;
  int       pid;        // -1 = CPU idle
  std::string policySnapshot;
};

Field Name
C++ Type
Size
R/W
Description
Constraints
tick
uint64_t
8
R
The clock tick this entry records.
Monotonically increasing. Matches currentTick at time of write.
pid
int
4
R
PID of the process that ran during this tick. -1 means CPU was idle.
-1 or valid pid in processTable.
policySnapshot
std::string
var
R
Name of the active scheduling policy at this tick. Allows Gantt chart to show policy switches.
Non-empty string matching a SchedulingPolicy name.

5.2 SchedulingMetrics
Defined in: engine/modules/scheduler/SchedulingMetrics.h   |   Stored in: SimulationState::metrics   |   Updated: each tick
Aggregated performance metrics computed and updated by the Scheduler module. These are the key output values measured against in the test plan and displayed in the React MetricsPanel.

struct SchedulingMetrics {
  float    avgWaitingTime;
  float    avgTurnaroundTime;
  float    cpuUtilization;
  uint32_t totalContextSwitches;
  uint32_t throughput;
  uint32_t completedProcesses;
  uint32_t totalProcesses;
};

Field Name
C++ Type
Size
R/W
Description
Constraints
avgWaitingTime
float
4
W
Mean waiting time across all TERMINATED processes. Sum of waitingTime / completedProcesses.
>= 0.0. 0.0 if no processes completed yet.
avgTurnaroundTime
float
4
W
Mean turnaround time across all TERMINATED processes. Sum of turnaroundTime / completedProcesses.
>= 0.0. Always >= avgWaitingTime.
cpuUtilization
float
4
W
Percentage of ticks the CPU was occupied (not idle). (busyTicks / currentTick) * 100.
0.0 to 100.0.
totalContextSwitches
uint32_t
4
W
Sum of contextSwitches across all processes (active and terminated).
>= 0.
throughput
uint32_t
4
W
Number of processes completed per 100 ticks (integer approximation).
>= 0.
completedProcesses
uint32_t
4
W
Count of processes that have reached TERMINATED state.
>= 0.
totalProcesses
uint32_t
4
W
Total processes created since last reset, including currently active.
>= completedProcesses.

6. Synchronisation Data Structures
6.1 Mutex
Defined in: engine/modules/sync/Mutex.h   |   Stored in: SimulationState::mutexTable
A simulated mutex providing mutual exclusion. Exactly one process may hold a mutex at a time. Any other process that attempts acquire() while the mutex is locked is moved to WAITING and placed in the blocked queue for this mutex.

struct Mutex {
  int              mutexId;
  std::string      name;
  bool             locked;
  int              ownerPid;
  std::deque<int>  waitingPids;
  uint64_t         lockedAtTick;
  uint32_t         totalAcquisitions;
  uint32_t         totalContentions;
};

Field Name
C++ Type
Size
R/W
Description
Constraints
mutexId
int
4
R
Unique identifier for this mutex, assigned at creation.
mutexId >= 1. Unique across mutexTable.
name
std::string
var
R
Human-readable label for the mutex. Shown in the SyncPanel UI.
Max 32 characters. Non-empty.
locked
bool
1
W
True if the mutex is currently held by a process. False if it is free.
true or false.
ownerPid
int
4
W
PID of the process currently holding this mutex. -1 if mutex is unlocked.
-1 or valid active pid.
waitingPids
deque<int>
var
W
Queue of PIDs blocked waiting to acquire this mutex. Ordered by arrival time (FIFO).
All PIDs must be valid and in WAITING state.
lockedAtTick
uint64_t
8
W
The clock tick at which the mutex was last acquired. 0 if currently unlocked.
0 if unlocked. Otherwise <= currentTick.
totalAcquisitions
uint32_t
4
W
Cumulative count of successful lock acquisitions for this mutex since simulation start.
>= 0.
totalContentions
uint32_t
4
W
Count of times a process was blocked because this mutex was already locked (contention events).
>= 0.

6.2 Semaphore
Defined in: engine/modules/sync/Semaphore.h   |   Stored in: SimulationState::semaphoreTable
A simulated counting semaphore. The value represents the number of available permits. wait() decrements the value; if it would go below 0 the calling process is blocked. signal() increments the value and unblocks one waiting process if any exist.

struct Semaphore {
  int              semId;
  std::string      name;
  SyncPrimitiveType  primitiveType;
  int              value;
  int              initialValue;
  std::deque<int>  waitingPids;
  uint32_t         totalWaits;
  uint32_t         totalSignals;
  uint32_t         totalBlocks;
};

Field Name
C++ Type
Size
R/W
Description
Constraints
semId
int
4
R
Unique semaphore identifier.
semId >= 1. Unique across semaphoreTable.
name
std::string
var
R
Human-readable label shown in UI.
Max 32 characters. Non-empty.
primitiveType
SyncPrimitiveType
4
R
SEMAPHORE_BINARY or SEMAPHORE_COUNTING. Determines initialValue enforcement.
Must be a semaphore variant of SyncPrimitiveType.
value
int
4
W
Current semaphore count. Decremented on wait(), incremented on signal().
For BINARY: 0 or 1 only. For COUNTING: 0 <= value <= initialValue.
initialValue
int
4
R
The value this semaphore was initialised with.
1 for BINARY. >= 1 for COUNTING.
waitingPids
deque<int>
var
W
Queue of PIDs blocked on this semaphore. Unblocked FIFO on signal().
All PIDs must be valid and in WAITING state.
totalWaits
uint32_t
4
W
Cumulative count of wait() calls on this semaphore.
>= 0.
totalSignals
uint32_t
4
W
Cumulative count of signal() calls.
>= 0.
totalBlocks
uint32_t
4
W
Count of times a wait() caused a process to block (value was 0).
>= 0. <= totalWaits.

7. Memory Management Data Structures
7.1 PageTableEntry (PTE)
Defined in: engine/modules/memory/PageTable.h   |   One per virtual page per process
A Page Table Entry maps one virtual page number to a physical frame number. Each process has one PTE per virtual page it owns. The PTE also tracks validity and reference metadata used by page replacement algorithms.

struct PageTableEntry {
  uint32_t  virtualPageNumber;
  int       frameNumber;
  bool      valid;
  bool      dirty;
  bool      referenced;
  uint64_t  loadTick;
  uint64_t  lastAccessTick;
};

Field Name
C++ Type
Size
R/W
Description
Constraints
virtualPageNumber
uint32_t
4
R
The virtual page number this entry maps. Index into the process's page table array.
0 <= VPN < memoryRequirement of parent PCB.
frameNumber
int
4
W
Physical frame number this page is loaded into. -1 if the page is not currently in memory.
-1 (not present) or 0 <= frameNumber < totalFrames.
valid
bool
1
W
True if this page is currently loaded in a physical frame. False triggers page fault on access.
true if and only if frameNumber != -1.
dirty
bool
1
W
True if this page has been written to since being loaded. Reserved for future write-back logic.
false on load. Set true on simulated write access.
referenced
bool
1
W
True if this page was accessed during the current tick. Cleared at start of each tick.
Reset to false on every tick start.
loadTick
uint64_t
8
W
The clock tick at which this page was most recently loaded into a frame. Used by FIFO policy.
0 if not currently loaded.
lastAccessTick
uint64_t
8
W
The clock tick of the most recent access to this page. Updated on every memory reference.
0 if never accessed. <= currentTick.

7.2 PageTable
Defined in: engine/modules/memory/PageTable.h   |   One per process   |   Stored in: SimulationState::pageTables

struct PageTable {
  int                        ownerPid;
  uint32_t                   pageSize;
  std::vector<PageTableEntry> entries;
};

Field Name
C++ Type
Size
R/W
Description
Constraints
ownerPid
int
4
R
PID of the process this page table belongs to.
Must be a valid pid in processTable.
pageSize
uint32_t
4
R
Size of each page in simulated bytes. Configured at simulation start, uniform for all processes.
Power of 2. Default: 256. Range: 64 to 1024.
entries
vector<PTE>
var
W
Array of PageTableEntries indexed by virtual page number. entries[VPN] gives the PTE for VPN.
Length == ownerPCB.memoryRequirement.

7.3 FrameTableEntry
Defined in: engine/modules/memory/FrameTable.h   |   One per physical frame
Represents one physical memory frame in the simulated RAM. The Frame Table tracks which page from which process currently occupies each frame.

struct FrameTableEntry {
  int       frameNumber;
  bool      occupied;
  int       ownerPid;
  uint32_t  virtualPageNumber;
  uint64_t  loadTick;
  uint64_t  lastAccessTick;
};

Field Name
C++ Type
Size
R/W
Description
Constraints
frameNumber
int
4
R
The physical frame number. Index into the frame table array.
0 <= frameNumber < totalFrames (configured at start).
occupied
bool
1
W
True if a page is currently loaded in this frame.
true iff ownerPid != -1.
ownerPid
int
4
W
PID of the process whose page occupies this frame. -1 if frame is free.
-1 or valid active pid.
virtualPageNumber
uint32_t
4
W
VPN of the page from ownerPid's address space that occupies this frame.
Valid only when occupied == true.
loadTick
uint64_t
8
W
Tick at which the current page was loaded. Used by FIFO replacement policy.
0 if not occupied.
lastAccessTick
uint64_t
8
W
Most recent tick at which this frame was accessed. Used by LRU replacement policy.
0 if not occupied.

7.4 MemoryMetrics
Defined in: engine/modules/memory/MemoryMetrics.h   |   Stored in: SimulationState::memMetrics

struct MemoryMetrics {
  uint32_t  totalFrames;
  uint32_t  occupiedFrames;
  uint32_t  totalPageFaults;
  float     pageFaultRate;
  uint32_t  totalReplacements;
  std::string activePolicy;
};

Field Name
C++ Type
Size
R/W
Description
Constraints
totalFrames
uint32_t
4
R
Total physical frames in simulated RAM. Configured at simulation start.
>= 4. Immutable during simulation.
occupiedFrames
uint32_t
4
W
Number of frames currently holding a page. Updated on every load/eviction.
0 <= occupiedFrames <= totalFrames.
totalPageFaults
uint32_t
4
W
Cumulative page faults across all processes since simulation start.
>= 0.
pageFaultRate
float
4
W
Page faults per 100 memory accesses. (totalPageFaults / totalMemAccesses) * 100.
0.0 to 100.0.
totalReplacements
uint32_t
4
W
Count of page evictions triggered by replacement algorithm.
>= 0. <= totalPageFaults.
activePolicy
std::string
var
W
Name of the currently active replacement policy. Matches ReplacementPolicy enum name.
"FIFO" or "LRU".

8. Event Bus and Logging Structures
8.1 SimEvent
Defined in: engine/core/EventBus.h   |   Published by all modules, consumed by API Bridge
Every notable state change in the simulator is published as a SimEvent. The API bridge collects all events produced during a tick and includes them in the WebSocket broadcast packet. The frontend renders them in the Decision Log panel.

struct SimEvent {
  uint64_t     tick;
  std::string  eventType;
  int          sourcePid;
  int          targetPid;
  int          resourceId;
  std::string  description;
};

Field Name
C++ Type
Size
R/W
Description
Constraints
tick
uint64_t
8
R
The clock tick at which this event occurred.
>= 0. Set by publisher.
eventType
std::string
var
R
Event type identifier. Must match one of the 13 event types defined in the SDD Event Bus table.
Non-empty. One of the defined event type strings.
sourcePid
int
4
R
PID of the process that caused this event. -1 for system-level events (e.g. TICK_ADVANCED).
-1 or valid pid.
targetPid
int
4
R
PID of the process affected by this event (e.g. the process that was unblocked). -1 if not applicable.
−1 or valid pid.
resourceId
int
4
R
ID of the mutex, semaphore, or page involved in this event. -1 if not applicable.
-1 or valid resource id.
description
std::string
var
R
Plain-English human-readable description of the event. Displayed directly in the Decision Log panel.
Non-empty. Max 200 characters. Must be self-explanatory to an OS student.

8.2 ProcessSpec (REST Payload)
Defined in: engine/bridge/RestServer.h   |   Received via POST /process/create
The JSON payload sent by the React frontend when a user manually creates a process. The API bridge deserialises this into a ProcessSpec and passes it to the Process Manager.

struct ProcessSpec {
  std::string   name;
  ProcessType   type;
  int           priority;
  uint32_t      cpuBurst;
  uint32_t      ioBurstDuration;
  uint32_t      memoryRequirement;
};

Field Name
C++ Type
Size
R/W
Description
Constraints
name
std::string
var
R
Optional user-supplied name. If empty, engine auto-generates 'proc_N'.
Max 32 chars. May be empty.
type
ProcessType
4
R
Workload type — CPU_BOUND, IO_BOUND, or MIXED.
Must be a valid ProcessType enum value.
priority
int
4
R
Scheduling priority. 1 = highest, 10 = lowest.
1 to 10 inclusive.
cpuBurst
uint32_t
4
R
Total CPU ticks required. If 0, engine assigns a random value based on process type.
>= 0. 0 = auto-assign.
ioBurstDuration
uint32_t
4
R
Duration of each I/O burst. 0 for CPU_BOUND. If 0 for MIXED, engine assigns default.
>= 0.
memoryRequirement
uint32_t
4
R
Number of virtual pages needed. If 0, engine assigns default based on type.
>= 0. 0 = auto-assign.

8.3 WorkloadScenario
Defined in: engine/bridge/WorkloadLoader.h   |   Selected via POST /workload/load
Pre-built workload configurations used to quickly populate the simulation with a realistic mix of processes. The WorkloadLoader instantiates a set of ProcessSpecs based on the chosen scenario.

Scenario
Process Count
Composition
Purpose
cpu_bound
5 processes
All CPU_BOUND
Stresses the scheduler. Good for comparing FCFS vs RR.
io_bound
5 processes
All IO_BOUND
Shows high wait times, low CPU util. Tests I/O handling.
mixed
8 processes
3 CPU + 3 IO + 2 MIXED
Realistic general workload. Best for policy comparison.

9. Naming Conventions and JSON Mapping
All C++ field names use camelCase. When serialised to JSON for the WebSocket packet or REST responses, field names use snake_case (automatically handled by the nlohmann/json serialisation macros). The mapping rule is straightforward:

C++ Field Name
JSON Key
Example Value
pid
pid
3
arrivalTick
arrival_tick
42
remainingBurst
remaining_burst
7
totalPageFaults
total_page_faults
15
avgWaitingTime
avg_waiting_time
4.33
cpuUtilization
cpu_utilization
87.5
blockedOnSyncId
blocked_on_sync_id
-1
lastAccessTick
last_access_tick
1018

Enum values are serialised as uppercase strings matching the enum name. Example: ProcessState::RUNNING serialises to the JSON string "RUNNING".

9.1 Null and Default Values
Condition
C++ Value
JSON Representation
No process running
runningPID = -1
"running_pid": -1
Process not started
startTick = 0
"start_tick": 0
Frame not occupied
ownerPid = -1
"owner_pid": -1
Thread not blocked
blockedOnSyncId = -1
"blocked_on_sync_id": -1
Page not in memory
frameNumber = -1
"frame_number": -1
