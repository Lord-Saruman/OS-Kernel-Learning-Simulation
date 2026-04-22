#pragma once

/**
 * StateSerializer.h — JSON Serialization for SimulationState
 *
 * Reference: DataDictionary §9 (Naming Conventions and JSON Mapping)
 *            SDD §5.3 (WebSocket State Stream packet format)
 *
 * Converts the entire SimulationState struct to a nlohmann::json object
 * suitable for WebSocket broadcast and GET /state/snapshot.
 *
 * Naming rule: camelCase C++ field → snake_case JSON key
 * Enum rule:   Serialized as uppercase strings matching enum name
 */

#include <nlohmann/json.hpp>

#include "core/SimulationState.h"
#include "core/SimEnums.h"
#include "core/SimEvent.h"
#include "core/DecisionLogEntry.h"
#include "core/EventBus.h"

namespace StateSerializer {

// ── Individual struct serializers ────────────────────────────

inline nlohmann::json serializePCB(const PCB& pcb) {
    return {
        {"pid",                pcb.pid},
        {"name",               pcb.name},
        {"type",               toString(pcb.type)},
        {"priority",           pcb.priority},
        {"state",              toString(pcb.state)},
        {"arrival_tick",       pcb.arrivalTick},
        {"start_tick",         pcb.startTick},
        {"termination_tick",   pcb.terminationTick},
        {"total_cpu_burst",    pcb.totalCpuBurst},
        {"remaining_burst",    pcb.remainingBurst},
        {"quantum_used",       pcb.quantumUsed},
        {"cpu_segment_length", pcb.cpuSegmentLength},
        {"io_burst_duration",  pcb.ioBurstDuration},
        {"io_remaining_ticks", pcb.ioRemainingTicks},
        {"io_completion_tick", pcb.ioCompletionTick},
        {"memory_requirement", pcb.memoryRequirement},
        {"page_table_id",      pcb.pageTableId},
        {"waiting_time",       pcb.waitingTime},
        {"turnaround_time",    pcb.turnaroundTime},
        {"context_switches",   pcb.contextSwitches},
        {"page_fault_count",   pcb.pageFaultCount},
        {"thread_ids",         pcb.threadIds}
    };
}

inline nlohmann::json serializeTCB(const TCB& tcb) {
    return {
        {"tid",                tcb.tid},
        {"parent_pid",         tcb.parentPid},
        {"state",              toString(tcb.state)},
        {"creation_tick",      tcb.creationTick},
        {"stack_size",         tcb.stackSize},
        {"simulated_sp",       tcb.simulatedSP},
        {"cpu_burst",          tcb.cpuBurst},
        {"remaining_burst",    tcb.remainingBurst},
        {"blocked_on_sync_id", tcb.blockedOnSyncId},
        {"waiting_time",       tcb.waitingTime}
    };
}

inline nlohmann::json serializeGanttEntry(const GanttEntry& g) {
    return {
        {"tick",            g.tick},
        {"pid",             g.pid},
        {"policy_snapshot", g.policySnapshot}
    };
}

inline nlohmann::json serializeSchedulingMetrics(const SchedulingMetrics& m) {
    return {
        {"avg_waiting_time",      m.avgWaitingTime},
        {"avg_turnaround_time",   m.avgTurnaroundTime},
        {"cpu_utilization",       m.cpuUtilization},
        {"total_context_switches", m.totalContextSwitches},
        {"throughput",            m.throughput},
        {"completed_processes",   m.completedProcesses},
        {"total_processes",       m.totalProcesses}
    };
}

inline nlohmann::json serializeMutex(const Mutex& mx) {
    return {
        {"mutex_id",           mx.mutexId},
        {"name",               mx.name},
        {"locked",             mx.locked},
        {"owner_pid",          mx.ownerPid},
        {"waiting_pids",       nlohmann::json(mx.waitingPids)},
        {"locked_at_tick",     mx.lockedAtTick},
        {"total_acquisitions", mx.totalAcquisitions},
        {"total_contentions",  mx.totalContentions}
    };
}

inline nlohmann::json serializeSemaphore(const Semaphore& sem) {
    return {
        {"sem_id",         sem.semId},
        {"name",           sem.name},
        {"primitive_type", toString(sem.primitiveType)},
        {"value",          sem.value},
        {"initial_value",  sem.initialValue},
        {"waiting_pids",   nlohmann::json(sem.waitingPids)},
        {"total_waits",    sem.totalWaits},
        {"total_signals",  sem.totalSignals},
        {"total_blocks",   sem.totalBlocks}
    };
}

inline nlohmann::json serializePageTableEntry(const PageTableEntry& pte) {
    return {
        {"virtual_page_number", pte.virtualPageNumber},
        {"frame_number",        pte.frameNumber},
        {"valid",               pte.valid},
        {"dirty",               pte.dirty},
        {"referenced",          pte.referenced},
        {"load_tick",           pte.loadTick},
        {"last_access_tick",    pte.lastAccessTick}
    };
}

inline nlohmann::json serializePageTable(const PageTable& pt) {
    nlohmann::json entries = nlohmann::json::array();
    for (const auto& pte : pt.entries) {
        entries.push_back(serializePageTableEntry(pte));
    }
    return {
        {"owner_pid",  pt.ownerPid},
        {"page_size",  pt.pageSize},
        {"entries",    entries}
    };
}

inline nlohmann::json serializeFrameTableEntry(const FrameTableEntry& fte) {
    return {
        {"frame_number",       fte.frameNumber},
        {"occupied",           fte.occupied},
        {"owner_pid",          fte.ownerPid},
        {"virtual_page_number", fte.virtualPageNumber},
        {"load_tick",          fte.loadTick},
        {"last_access_tick",   fte.lastAccessTick}
    };
}

inline nlohmann::json serializeMemoryMetrics(const MemoryMetrics& mm) {
    return {
        {"total_frames",       mm.totalFrames},
        {"occupied_frames",    mm.occupiedFrames},
        {"total_page_faults",  mm.totalPageFaults},
        {"page_fault_rate",    mm.pageFaultRate},
        {"total_replacements", mm.totalReplacements},
        {"active_policy",      mm.activePolicy},
        {"frames_free",        mm.totalFrames - mm.occupiedFrames}
    };
}

inline nlohmann::json serializeSimEvent(const SimEvent& ev) {
    return {
        {"tick",        ev.tick},
        {"event_type",  ev.eventType},
        {"source_pid",  ev.sourcePid},
        {"target_pid",  ev.targetPid},
        {"resource_id", ev.resourceId},
        {"description", ev.description}
    };
}

inline nlohmann::json serializeDecisionLogEntry(const DecisionLogEntry& d) {
    return {
        {"tick",    d.tick},
        {"message", d.message}
    };
}

// ── Full state serializer ────────────────────────────────────

/**
 * Serialize the entire SimulationState + tick events to JSON.
 * This produces the WebSocket state packet defined in SDD §5.3.
 *
 * IMPORTANT: Caller must hold at least a shared_lock on state.stateMutex.
 */
inline nlohmann::json serializeState(const SimulationState& state,
                                      const EventBus& bus) {
    nlohmann::json j;

    // Clock
    j["tick"]             = state.currentTick;
    j["status"]           = toString(state.status);
    j["mode"]             = toString(state.mode);
    j["auto_speed_ms"]    = state.autoSpeedMs;

    // Scheduler top-level
    j["running_pid"]      = state.runningPID;
    j["ready_queue"]      = nlohmann::json(state.readyQueue);
    j["active_policy"]    = state.activePolicy;
    j["time_quantum"]     = state.timeQuantum;

    // Memory top-level
    j["active_replacement"] = state.activeReplacement;

    // Processes
    nlohmann::json procs = nlohmann::json::array();
    for (const auto& [pid, pcb] : state.processTable) {
        procs.push_back(serializePCB(pcb));
    }
    j["processes"] = procs;

    // Threads
    nlohmann::json threads = nlohmann::json::array();
    for (const auto& [tid, tcb] : state.threadTable) {
        threads.push_back(serializeTCB(tcb));
    }
    j["threads"] = threads;

    // Gantt log
    nlohmann::json gantt = nlohmann::json::array();
    for (const auto& g : state.ganttLog) {
        gantt.push_back(serializeGanttEntry(g));
    }
    j["gantt_log"] = gantt;

    // Scheduling metrics
    j["metrics"] = serializeSchedulingMetrics(state.metrics);

    // Frame table
    nlohmann::json frames = nlohmann::json::array();
    for (const auto& fte : state.frameTable) {
        frames.push_back(serializeFrameTableEntry(fte));
    }
    j["frame_table"] = frames;

    // Page tables
    nlohmann::json pts = nlohmann::json::object();
    for (const auto& [pid, pt] : state.pageTables) {
        pts[std::to_string(pid)] = serializePageTable(pt);
    }
    j["page_tables"] = pts;

    // Mutex table
    nlohmann::json mutexes = nlohmann::json::object();
    for (const auto& [id, mx] : state.mutexTable) {
        mutexes[std::to_string(id)] = serializeMutex(mx);
    }
    j["mutex_table"] = mutexes;

    // Semaphore table
    nlohmann::json sems = nlohmann::json::object();
    for (const auto& [id, sem] : state.semaphoreTable) {
        sems[std::to_string(id)] = serializeSemaphore(sem);
    }
    j["semaphore_table"] = sems;

    // Blocked queues
    nlohmann::json bqs = nlohmann::json::object();
    for (const auto& [syncId, pids] : state.blockedQueues) {
        bqs[std::to_string(syncId)] = nlohmann::json(pids);
    }
    j["blocked_queues"] = bqs;

    // Memory metrics
    j["mem_metrics"] = serializeMemoryMetrics(state.memMetrics);

    // Tick events (from EventBus)
    nlohmann::json events = nlohmann::json::array();
    for (const auto& ev : bus.getTickEvents()) {
        events.push_back(serializeSimEvent(ev));
    }
    j["events"] = events;

    // Decision log
    nlohmann::json dlog = nlohmann::json::array();
    for (const auto& d : state.decisionLog) {
        dlog.push_back(serializeDecisionLogEntry(d));
    }
    j["decision_log"] = dlog;

    return j;
}

} // namespace StateSerializer
