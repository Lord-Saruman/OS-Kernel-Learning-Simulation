/**
 * SimState.ts — TypeScript interfaces matching the JSON packet
 * from the C++ StateSerializer (Phase 7).
 *
 * Reference: DataDictionary §9 (JSON Mapping)
 *            SDD §5.3 (WebSocket State Stream)
 *
 * All field names are snake_case matching the JSON keys.
 */

// ── Process Control Block ────────────────────────────────────
export interface PCB {
  pid: number;
  name: string;
  type: string;
  priority: number;
  state: string;
  arrival_tick: number;
  start_tick: number;
  termination_tick: number;
  total_cpu_burst: number;
  remaining_burst: number;
  quantum_used: number;
  cpu_segment_length: number;
  io_burst_duration: number;
  io_remaining_ticks: number;
  io_completion_tick: number;
  memory_requirement: number;
  page_table_id: number;
  waiting_time: number;
  turnaround_time: number;
  context_switches: number;
  page_fault_count: number;
  thread_ids: number[];
}

// ── Thread Control Block ─────────────────────────────────────
export interface TCB {
  tid: number;
  parent_pid: number;
  state: string;
  creation_tick: number;
  stack_size: number;
  simulated_sp: number;
  cpu_burst: number;
  remaining_burst: number;
  blocked_on_sync_id: number;
  waiting_time: number;
}

// ── Gantt Chart Entry ────────────────────────────────────────
export interface GanttEntry {
  tick: number;
  pid: number;
  policy_snapshot: string;
}

// ── Scheduling Metrics ───────────────────────────────────────
export interface SchedulingMetrics {
  avg_waiting_time: number;
  avg_turnaround_time: number;
  cpu_utilization: number;
  total_context_switches: number;
  throughput: number;
  completed_processes: number;
  total_processes: number;
}

// ── Frame Table Entry ────────────────────────────────────────
export interface FrameTableEntry {
  frame_number: number;
  occupied: boolean;
  owner_pid: number;
  virtual_page_number: number;
  load_tick: number;
  last_access_tick: number;
}

// ── Page Table Entry ─────────────────────────────────────────
export interface PageTableEntry {
  virtual_page_number: number;
  frame_number: number;
  valid: boolean;
  dirty: boolean;
  referenced: boolean;
  load_tick: number;
  last_access_tick: number;
}

// ── Page Table ───────────────────────────────────────────────
export interface PageTable {
  owner_pid: number;
  page_size: number;
  entries: PageTableEntry[];
}

// ── Mutex ────────────────────────────────────────────────────
export interface MutexState {
  mutex_id: number;
  name: string;
  locked: boolean;
  owner_pid: number;
  waiting_pids: number[];
  locked_at_tick: number;
  total_acquisitions: number;
  total_contentions: number;
}

// ── Semaphore ────────────────────────────────────────────────
export interface SemaphoreState {
  sem_id: number;
  name: string;
  primitive_type: string;
  value: number;
  initial_value: number;
  waiting_pids: number[];
  total_waits: number;
  total_signals: number;
  total_blocks: number;
}

// ── Memory Metrics ───────────────────────────────────────────
export interface MemoryMetrics {
  total_frames: number;
  occupied_frames: number;
  total_page_faults: number;
  page_fault_rate: number;
  total_replacements: number;
  active_policy: string;
  frames_free: number;
}

// ── Sim Event ────────────────────────────────────────────────
export interface SimEvent {
  tick: number;
  event_type: string;
  source_pid: number;
  target_pid: number;
  resource_id: number;
  description: string;
}

// ── Decision Log Entry ───────────────────────────────────────
export interface DecisionLogEntry {
  tick: number;
  message: string;
}

// ── Full Simulation State (WebSocket packet) ─────────────────
export interface SimState {
  tick: number;
  status: string;
  mode: string;
  auto_speed_ms: number;
  running_pid: number;
  ready_queue: number[];
  active_policy: string;
  time_quantum: number;
  active_replacement: string;
  processes: PCB[];
  threads: TCB[];
  gantt_log: GanttEntry[];
  metrics: SchedulingMetrics;
  frame_table: FrameTableEntry[];
  page_tables: Record<string, PageTable>;
  mutex_table: Record<string, MutexState>;
  semaphore_table: Record<string, SemaphoreState>;
  blocked_queues: Record<string, number[]>;
  mem_metrics: MemoryMetrics;
  events: SimEvent[];
  decision_log: DecisionLogEntry[];
}

// ── Initial empty state ──────────────────────────────────────
export const INITIAL_STATE: SimState = {
  tick: 0,
  status: 'IDLE',
  mode: 'STEP',
  auto_speed_ms: 500,
  running_pid: -1,
  ready_queue: [],
  active_policy: 'FCFS',
  time_quantum: 2,
  active_replacement: 'FIFO',
  processes: [],
  threads: [],
  gantt_log: [],
  metrics: {
    avg_waiting_time: 0,
    avg_turnaround_time: 0,
    cpu_utilization: 0,
    total_context_switches: 0,
    throughput: 0,
    completed_processes: 0,
    total_processes: 0,
  },
  frame_table: [],
  page_tables: {},
  mutex_table: {},
  semaphore_table: {},
  blocked_queues: {},
  mem_metrics: {
    total_frames: 16,
    occupied_frames: 0,
    total_page_faults: 0,
    page_fault_rate: 0,
    total_replacements: 0,
    active_policy: 'FIFO',
    frames_free: 16,
  },
  events: [],
  decision_log: [],
};
