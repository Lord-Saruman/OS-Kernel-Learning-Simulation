#pragma once

/**
 * Scheduler.h — CPU Scheduler Module
 *
 * Reference: SDD Section 3.1 (Scheduler Module), SDD Section 3.4 (Strategy),
 *            PRD Section 6.2 (FR-SC-01 through FR-SC-06),
 *            DataDictionary Sections 2.5, 5.1, 5.2
 *
 * The CPU Scheduler is the second OS subsystem module. It owns the ready
 * queue dispatch logic: selecting the next process to run based on the
 * active policy, managing the time quantum (RR), enforcing preemption,
 * recording Gantt chart entries, and computing scheduling metrics.
 *
 * Policies are injected as strategy objects (ISchedulingPolicy). Swapping
 * a policy at runtime is a single pointer reassignment — zero changes
 * to module logic. (FR-SC-04: hot-swappable)
 *
 * Responsibilities:
 *   - READY → RUNNING (dispatch via active policy)
 *   - RUNNING → READY (quantum expiry in RR, preemption in PRIORITY_P)
 *   - RUNNING → WAITING (I/O initiation via cpuSegmentLength cycle)
 *   - RUNNING → TERMINATED (burst completion)
 *   - Gantt chart logging (one entry per tick)
 *   - SchedulingMetrics computation
 *   - Context switch detection and counting
 *
 * Tick execution order (SDD §3.2):
 *   Process Manager runs BEFORE the Scheduler on each tick.
 *   The Scheduler runs AFTER newly-arrived processes are already in readyQueue.
 */

#include <string>
#include <memory>
#include <cstdint>

#include "core/ISimModule.h"
#include "modules/scheduler/ISchedulingPolicy.h"

// Forward declarations
struct SimulationState;
class EventBus;

class Scheduler : public ISimModule {
public:
    Scheduler();

    // ══════════════════════════════════════════════════════════
    // ISimModule interface
    // ══════════════════════════════════════════════════════════

    /**
     * Called once per clock tick by the ClockController.
     *
     * Sequence:
     *   1. handleIOInitiation    — CPU→I/O burst cycle transition
     *   2. handleQuantumExpiry   — RR quantum enforcement
     *   3. checkPreemption       — Priority preemptive check
     *   4. dispatchProcess       — select next if CPU idle
     *   5. decrementBurst        — decrement running process burst
     *   6. logGanttEntry         — record this tick's CPU owner
     *   7. updateMetrics         — recompute SchedulingMetrics
     */
    void onTick(SimulationState& state, EventBus& bus) override;

    /**
     * Reset module to initial state. Called on simulation reset.
     */
    void reset() override;

    /**
     * Get the current operational status of this module.
     */
    ModuleStatus getStatus() const override;

    /**
     * Get the human-readable name of this module.
     */
    std::string getModuleName() const override;

    // ══════════════════════════════════════════════════════════
    // Policy Management API
    // (called by API Bridge in Phase 7, or directly in tests)
    // ══════════════════════════════════════════════════════════

    /**
     * Swap the active scheduling policy at runtime.
     * Accepts: "FCFS", "ROUND_ROBIN", "PRIORITY_NP", "PRIORITY_P"
     *
     * @param policyName  String name matching SchedulingPolicy enum
     */
    void setPolicy(const std::string& policyName);

    /**
     * Get the name of the currently active scheduling policy.
     */
    std::string getActivePolicyName() const;

private:
    ModuleStatus status_;
    std::unique_ptr<ISchedulingPolicy> activePolicy_;
    int previousRunningPID_;    // For context switch detection
    uint64_t busyTicks_;        // Count of ticks CPU was occupied (for utilization)

    // ══════════════════════════════════════════════════════════
    // Per-tick steps — called in order by onTick()
    // ══════════════════════════════════════════════════════════

    /**
     * If the running process has completed its CPU segment,
     * initiate I/O burst (RUNNING → WAITING).
     */
    void handleIOInitiation(SimulationState& state, EventBus& bus);

    /**
     * If RR policy is active and quantum expired, preempt the
     * running process (RUNNING → READY, push to back of queue).
     */
    void handleQuantumExpiry(SimulationState& state, EventBus& bus);

    /**
     * If PRIORITY_P is active, check if any process in the ready
     * queue has higher priority than the running process.
     * If so, preempt (RUNNING → READY, insert at front of queue).
     */
    void checkPreemption(SimulationState& state, EventBus& bus);

    /**
     * If CPU is idle and readyQueue is non-empty, select the next
     * process via the active policy and dispatch it (READY → RUNNING).
     */
    void dispatchProcess(SimulationState& state, EventBus& bus);

    /**
     * Decrement the running process's remainingBurst and increment
     * quantumUsed. If burst reaches 0, terminate the process.
     */
    void decrementBurst(SimulationState& state, EventBus& bus);

    /**
     * Append a GanttEntry for this tick to state.ganttLog.
     */
    void logGanttEntry(SimulationState& state);

    /**
     * Recompute SchedulingMetrics from current state.
     */
    void updateMetrics(SimulationState& state);

    // ══════════════════════════════════════════════════════════
    // Helpers
    // ══════════════════════════════════════════════════════════

    /**
     * Factory method — creates a policy instance from name string.
     */
    static std::unique_ptr<ISchedulingPolicy> createPolicy(const std::string& name);
};
