#pragma once

/**
 * GanttEntry.h — Gantt Chart Log Entry
 *
 * Reference: DataDictionary_MiniOS_Simulator.md, Section 5.1
 * Appended to: SimulationState::ganttLog each tick
 *
 * One GanttEntry is appended every clock tick to record which process
 * (or idle) held the CPU. The frontend uses this to render the Gantt chart.
 */

#include <string>
#include <cstdint>

struct GanttEntry {
    uint64_t    tick;              // The clock tick this entry records
    int         pid;              // PID of running process (-1 = CPU idle)
    std::string policySnapshot;   // Active scheduling policy name at this tick

    // ── Default Constructor ──────────────────────────────────
    GanttEntry()
        : tick(0)
        , pid(-1)
        , policySnapshot("")
    {}

    GanttEntry(uint64_t t, int p, const std::string& policy)
        : tick(t)
        , pid(p)
        , policySnapshot(policy)
    {}
};
