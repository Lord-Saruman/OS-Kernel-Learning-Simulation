#pragma once

/**
 * MemoryManager.h — Memory Manager Module
 *
 * Reference: SDD Section 3.1 (Memory Manager Module), SDD Section 3.4 (Strategy),
 *            PRD Section 6.4 (FR-MM-01 through FR-MM-06),
 *            DataDictionary Sections 2.6, 7.1, 7.2, 7.3, 7.4
 *
 * The Memory Manager is the third OS subsystem module. It owns the frame
 * table and per-process page tables. It simulates paging-based virtual
 * memory with configurable page replacement policies (FIFO, LRU).
 *
 * Policies are injected as strategy objects (IReplacementPolicy). Swapping
 * a policy at runtime is a single pointer reassignment — zero changes to
 * module logic. (FR-MM-06: hot-swappable)
 *
 * Responsibilities:
 *   - Page table creation for newly admitted processes
 *   - Memory access simulation for the running process (one access per tick)
 *   - Page fault detection and handling
 *   - Frame allocation (free frame) and eviction (via replacement policy)
 *   - Frame cleanup when processes terminate
 *   - MemoryMetrics computation (page faults, fault rate, replacements)
 *   - Reference bit clearing at tick start
 *
 * Tick execution order (SDD §3.2):
 *   Process Manager runs BEFORE the Memory Manager.
 *   Scheduler runs BEFORE the Memory Manager.
 *   The Memory Manager services memory accesses for the currently running
 *   process as determined by the Scheduler.
 *
 * Memory access model:
 *   Each tick, the running process accesses one virtual page.
 *   By default, VPN is determined by round-robin scan:
 *       vpn = accessCounter % memoryRequirement
 *   For testing, an explicit access sequence can be set via
 *   setAccessSequence() to drive textbook reference strings.
 */

#include <string>
#include <memory>
#include <cstdint>
#include <vector>

#include "core/ISimModule.h"
#include "modules/memory/IReplacementPolicy.h"

// Forward declarations
struct SimulationState;
class EventBus;

class MemoryManager : public ISimModule {
public:
    MemoryManager();

    // ══════════════════════════════════════════════════════════
    // ISimModule interface
    // ══════════════════════════════════════════════════════════

    /**
     * Called once per clock tick by the ClockController.
     *
     * Sequence:
     *   1. initPageTablesForNewProcesses  — create page tables for admitted processes
     *   2. clearReferenceBits             — reset referenced flag on all PTEs
     *   3. simulateMemoryAccess           — access VPN for running process → fault or hit
     *   4. cleanupTerminatedProcessPages  — free frames for terminated processes
     *   5. updateMetrics                  — recompute MemoryMetrics
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

    /**
     * Bootstrap after construction or reset.
     * Initialises the frame table in SimulationState.
     */
    void bootstrap(SimulationState& state, uint32_t frameCount) override;

    // ══════════════════════════════════════════════════════════
    // Policy Management API
    // (called by API Bridge in Phase 7, or directly in tests)
    // ══════════════════════════════════════════════════════════

    /**
     * Swap the active page replacement policy at runtime.
     * Accepts: "FIFO", "LRU"
     *
     * @param policyName  String name matching ReplacementPolicy enum
     */
    void setPolicy(const std::string& policyName);

    /**
     * Get the name of the currently active replacement policy.
     */
    std::string getActivePolicyName() const;

    // ══════════════════════════════════════════════════════════
    // Frame Table Initialization
    // ══════════════════════════════════════════════════════════

    /**
     * Initialize the frame table with the given number of physical frames.
     * This should be called once at simulation start or after reset.
     * The frame count is customizable for different simulation scenarios.
     *
     * @param state       Shared simulation state
     * @param totalFrames Number of physical frames (default: 16, min: 4)
     */
    void initializeFrameTable(SimulationState& state, uint32_t totalFrames = 16);

    // ══════════════════════════════════════════════════════════
    // Test Support — Explicit Access Sequence
    // ══════════════════════════════════════════════════════════

    /**
     * Set an explicit sequence of VPNs to access on successive ticks.
     * Overrides the default round-robin pattern. When the sequence is
     * exhausted, falls back to round-robin.
     *
     * Used by unit tests to drive textbook page reference strings
     * for deterministic validation of FIFO/LRU fault counts.
     *
     * @param vpns  Ordered list of virtual page numbers to access
     */
    void setAccessSequence(const std::vector<uint32_t>& vpns);

private:
    ModuleStatus status_;
    std::unique_ptr<IReplacementPolicy> activePolicy_;
    uint64_t totalMemoryAccesses_;   // For pageFaultRate calculation
    uint64_t roundRobinCounter_;     // For default round-robin VPN selection

    // Explicit access sequence for testing
    std::vector<uint32_t> accessSequence_;
    size_t accessSequenceIdx_;

    // ══════════════════════════════════════════════════════════
    // Per-tick steps — called in order by onTick()
    // ══════════════════════════════════════════════════════════

    /**
     * Create page tables for processes that have been admitted (READY+)
     * but don't yet have an entry in state.pageTables.
     */
    void initPageTablesForNewProcesses(SimulationState& state, EventBus& bus);

    /**
     * Reset the 'referenced' bit on all page table entries to false.
     * Called at the start of each tick per DataDictionary §7.1.
     */
    void clearReferenceBits(SimulationState& state);

    /**
     * Simulate one memory access for the currently running process.
     * Determines VPN to access, checks page table, handles faults.
     */
    void simulateMemoryAccess(SimulationState& state, EventBus& bus);

    /**
     * Free all frames owned by terminated processes and remove
     * their page tables from state.pageTables.
     */
    void cleanupTerminatedProcessPages(SimulationState& state, EventBus& bus);

    /**
     * Recompute MemoryMetrics from current frame table state.
     */
    void updateMetrics(SimulationState& state);

    // ══════════════════════════════════════════════════════════
    // Helpers
    // ══════════════════════════════════════════════════════════

    /**
     * Find a free (unoccupied) frame in the frame table.
     * @return Frame index, or -1 if all frames are occupied
     */
    int findFreeFrame(const SimulationState& state);

    /**
     * Load a page into a frame, updating both the frame table entry
     * and the process's page table entry.
     */
    void loadPage(SimulationState& state, EventBus& bus, int pid,
                  uint32_t vpn, int frameIdx);

    /**
     * Evict the page currently in the given frame, invalidating
     * the old owner's page table entry.
     */
    void evictPage(SimulationState& state, EventBus& bus, int victimFrame);

    /**
     * Determine the VPN to access this tick for the given process.
     * Uses explicit access sequence if set, else round-robin.
     */
    uint32_t getNextVPN(uint32_t memoryRequirement);

    /**
     * Factory method — creates a policy instance from name string.
     */
    static std::unique_ptr<IReplacementPolicy> createPolicy(const std::string& name);
};
