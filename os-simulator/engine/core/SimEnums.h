#pragma once

/**
 * SimEnums.h — All enumeration types for the Mini OS Kernel Simulator
 *
 * Reference: DataDictionary_MiniOS_Simulator.md, Section 2 (§2.1–§2.8)
 * Location:  engine/core/SimEnums.h
 *
 * RULE: All enums are scoped (enum class) for type safety.
 *       Every enum value must match the Data Dictionary exactly.
 *       toString() / fromString() helpers provided for JSON serialisation.
 */

#include <string>
#include <stdexcept>

// ─────────────────────────────────────────────────────────────
// §2.1  ProcessState — Lifecycle state of a process
// Valid transitions defined in Process Manager LLD
// ─────────────────────────────────────────────────────────────
enum class ProcessState {
    NEW,          // Created but not yet admitted to ready queue
    READY,        // In ready queue, waiting for CPU allocation
    RUNNING,      // Currently executing on simulated CPU (only one at a time)
    WAITING,      // Blocked — waiting for I/O or sync primitive
    TERMINATED    // Completed or killed, PCB retained for metrics
};

inline std::string toString(ProcessState s) {
    switch (s) {
        case ProcessState::NEW:        return "NEW";
        case ProcessState::READY:      return "READY";
        case ProcessState::RUNNING:    return "RUNNING";
        case ProcessState::WAITING:    return "WAITING";
        case ProcessState::TERMINATED: return "TERMINATED";
    }
    return "UNKNOWN";
}

inline ProcessState processStateFromString(const std::string& s) {
    if (s == "NEW")        return ProcessState::NEW;
    if (s == "READY")      return ProcessState::READY;
    if (s == "RUNNING")    return ProcessState::RUNNING;
    if (s == "WAITING")    return ProcessState::WAITING;
    if (s == "TERMINATED") return ProcessState::TERMINATED;
    throw std::invalid_argument("Invalid ProcessState: " + s);
}

// ─────────────────────────────────────────────────────────────
// §2.2  ThreadState — Lifecycle state of a thread
// A thread inherits WAITING if its parent process enters WAITING
// ─────────────────────────────────────────────────────────────
enum class ThreadState {
    T_NEW,        // Created but not yet scheduled
    T_RUNNABLE,   // Ready to be scheduled within its process
    T_RUNNING,    // Currently executing (parent process is RUNNING)
    T_BLOCKED,    // Blocked on a synchronisation primitive
    T_TERMINATED  // Exited, TCB retained briefly then removed
};

inline std::string toString(ThreadState s) {
    switch (s) {
        case ThreadState::T_NEW:        return "T_NEW";
        case ThreadState::T_RUNNABLE:   return "T_RUNNABLE";
        case ThreadState::T_RUNNING:    return "T_RUNNING";
        case ThreadState::T_BLOCKED:    return "T_BLOCKED";
        case ThreadState::T_TERMINATED: return "T_TERMINATED";
    }
    return "UNKNOWN";
}

inline ThreadState threadStateFromString(const std::string& s) {
    if (s == "T_NEW")        return ThreadState::T_NEW;
    if (s == "T_RUNNABLE")   return ThreadState::T_RUNNABLE;
    if (s == "T_RUNNING")    return ThreadState::T_RUNNING;
    if (s == "T_BLOCKED")    return ThreadState::T_BLOCKED;
    if (s == "T_TERMINATED") return ThreadState::T_TERMINATED;
    throw std::invalid_argument("Invalid ThreadState: " + s);
}

// ─────────────────────────────────────────────────────────────
// §2.3  SimMode — Simulation advancement mode
// ─────────────────────────────────────────────────────────────
enum class SimMode {
    STEP,  // One tick at a time, waits for explicit /sim/step
    AUTO   // Advances automatically on timer (interval = autoSpeedMs)
};

inline std::string toString(SimMode m) {
    switch (m) {
        case SimMode::STEP: return "STEP";
        case SimMode::AUTO: return "AUTO";
    }
    return "UNKNOWN";
}

inline SimMode simModeFromString(const std::string& s) {
    if (s == "STEP") return SimMode::STEP;
    if (s == "AUTO") return SimMode::AUTO;
    throw std::invalid_argument("Invalid SimMode: " + s);
}

// ─────────────────────────────────────────────────────────────
// §2.4  SimStatus — Current simulation lifecycle status
//
// Lifecycle: IDLE → start → RUNNING → pause → PAUSED → start → RUNNING
//            Any state → reset → IDLE
//
// Note: STOPPED was removed in Phase 6 integration. The simulator
// resets directly to IDLE without an intermediate terminal state.
// ─────────────────────────────────────────────────────────────
enum class SimStatus {
    IDLE,     // Not started or has been reset
    RUNNING,  // Clock actively advancing ticks
    PAUSED    // Clock suspended, state preserved
};

inline std::string toString(SimStatus s) {
    switch (s) {
        case SimStatus::IDLE:    return "IDLE";
        case SimStatus::RUNNING: return "RUNNING";
        case SimStatus::PAUSED:  return "PAUSED";
    }
    return "UNKNOWN";
}

inline SimStatus simStatusFromString(const std::string& s) {
    if (s == "IDLE")    return SimStatus::IDLE;
    if (s == "RUNNING") return SimStatus::RUNNING;
    if (s == "PAUSED")  return SimStatus::PAUSED;
    throw std::invalid_argument("Invalid SimStatus: " + s);
}

// ─────────────────────────────────────────────────────────────
// §2.5  SchedulingPolicy — CPU scheduling algorithm identifier
// ─────────────────────────────────────────────────────────────
enum class SchedulingPolicy {
    FCFS,         // First-Come-First-Served, non-preemptive
    ROUND_ROBIN,  // Round Robin, preemptive, uses timeQuantum
    PRIORITY_NP,  // Priority, non-preemptive (lower value = higher priority)
    PRIORITY_P    // Priority, preemptive
};

inline std::string toString(SchedulingPolicy p) {
    switch (p) {
        case SchedulingPolicy::FCFS:        return "FCFS";
        case SchedulingPolicy::ROUND_ROBIN: return "ROUND_ROBIN";
        case SchedulingPolicy::PRIORITY_NP: return "PRIORITY_NP";
        case SchedulingPolicy::PRIORITY_P:  return "PRIORITY_P";
    }
    return "UNKNOWN";
}

inline SchedulingPolicy schedulingPolicyFromString(const std::string& s) {
    if (s == "FCFS")        return SchedulingPolicy::FCFS;
    if (s == "ROUND_ROBIN") return SchedulingPolicy::ROUND_ROBIN;
    if (s == "PRIORITY_NP") return SchedulingPolicy::PRIORITY_NP;
    if (s == "PRIORITY_P")  return SchedulingPolicy::PRIORITY_P;
    throw std::invalid_argument("Invalid SchedulingPolicy: " + s);
}

// ─────────────────────────────────────────────────────────────
// §2.6  ReplacementPolicy — Page replacement algorithm identifier
// ─────────────────────────────────────────────────────────────
enum class ReplacementPolicy {
    FIFO,  // First-In-First-Out — evicts frame with lowest loadTick
    LRU    // Least Recently Used — evicts frame with lowest lastAccessTick
};

inline std::string toString(ReplacementPolicy p) {
    switch (p) {
        case ReplacementPolicy::FIFO: return "FIFO";
        case ReplacementPolicy::LRU:  return "LRU";
    }
    return "UNKNOWN";
}

inline ReplacementPolicy replacementPolicyFromString(const std::string& s) {
    if (s == "FIFO") return ReplacementPolicy::FIFO;
    if (s == "LRU")  return ReplacementPolicy::LRU;
    throw std::invalid_argument("Invalid ReplacementPolicy: " + s);
}

// ─────────────────────────────────────────────────────────────
// §2.7  ProcessType — Workload profile classification
// ─────────────────────────────────────────────────────────────
enum class ProcessType {
    CPU_BOUND,  // High CPU burst, minimal I/O
    IO_BOUND,   // Short CPU burst, long I/O waits
    MIXED       // Alternating CPU and I/O bursts
};

inline std::string toString(ProcessType t) {
    switch (t) {
        case ProcessType::CPU_BOUND: return "CPU_BOUND";
        case ProcessType::IO_BOUND:  return "IO_BOUND";
        case ProcessType::MIXED:     return "MIXED";
    }
    return "UNKNOWN";
}

inline ProcessType processTypeFromString(const std::string& s) {
    if (s == "CPU_BOUND") return ProcessType::CPU_BOUND;
    if (s == "IO_BOUND")  return ProcessType::IO_BOUND;
    if (s == "MIXED")     return ProcessType::MIXED;
    throw std::invalid_argument("Invalid ProcessType: " + s);
}

// ─────────────────────────────────────────────────────────────
// §2.8  SyncPrimitiveType — Synchronisation primitive classification
// ─────────────────────────────────────────────────────────────
enum class SyncPrimitiveType {
    MUTEX,               // Binary mutual exclusion lock with ownership
    SEMAPHORE_BINARY,    // Counting semaphore initialised to 1 (no ownership)
    SEMAPHORE_COUNTING   // Counting semaphore initialised to N > 1
};

inline std::string toString(SyncPrimitiveType t) {
    switch (t) {
        case SyncPrimitiveType::MUTEX:              return "MUTEX";
        case SyncPrimitiveType::SEMAPHORE_BINARY:   return "SEMAPHORE_BINARY";
        case SyncPrimitiveType::SEMAPHORE_COUNTING: return "SEMAPHORE_COUNTING";
    }
    return "UNKNOWN";
}

inline SyncPrimitiveType syncPrimitiveTypeFromString(const std::string& s) {
    if (s == "MUTEX")              return SyncPrimitiveType::MUTEX;
    if (s == "SEMAPHORE_BINARY")   return SyncPrimitiveType::SEMAPHORE_BINARY;
    if (s == "SEMAPHORE_COUNTING") return SyncPrimitiveType::SEMAPHORE_COUNTING;
    throw std::invalid_argument("Invalid SyncPrimitiveType: " + s);
}
