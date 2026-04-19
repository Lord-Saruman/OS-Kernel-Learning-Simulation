#pragma once

/**
 * SchedulingMetrics.h — Aggregated Scheduling Performance Metrics
 *
 * Reference: DataDictionary_MiniOS_Simulator.md, Section 5.2
 * Stored in: SimulationState::metrics
 * Updated:   Each tick by the Scheduler module
 *
 * These are the key output values measured against in the test plan
 * and displayed in the React MetricsPanel.
 */

#include <cstdint>

struct SchedulingMetrics {
    float    avgWaitingTime;        // Mean waiting time across TERMINATED processes
    float    avgTurnaroundTime;     // Mean turnaround time across TERMINATED processes
    float    cpuUtilization;        // % of ticks CPU was occupied (0.0–100.0)
    uint32_t totalContextSwitches;  // Sum of context switches across all processes
    uint32_t throughput;            // Processes completed per 100 ticks (integer)
    uint32_t completedProcesses;    // Count of TERMINATED processes
    uint32_t totalProcesses;        // Total processes created since last reset

    // ── Default Constructor ──────────────────────────────────
    SchedulingMetrics()
        : avgWaitingTime(0.0f)
        , avgTurnaroundTime(0.0f)
        , cpuUtilization(0.0f)
        , totalContextSwitches(0)
        , throughput(0)
        , completedProcesses(0)
        , totalProcesses(0)
    {}
};
