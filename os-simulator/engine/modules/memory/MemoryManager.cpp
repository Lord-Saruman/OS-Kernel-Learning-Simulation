/**
 * MemoryManager.cpp — Memory Manager Module Implementation
 *
 * Reference: SDD Section 3.1, 3.4; PRD Section 6.4;
 *            DataDictionary Sections 2.6, 7.1, 7.2, 7.3, 7.4
 *
 * Phase 4 implementation of the Memory Manager — the third OS subsystem module.
 *
 * onTick() sequence per clock tick:
 *   1. initPageTablesForNewProcesses() — create page tables for admitted processes
 *   2. clearReferenceBits()            — reset referenced flags on all PTEs
 *   3. simulateMemoryAccess()          — access VPN for running process (fault or hit)
 *   4. cleanupTerminatedProcessPages() — free frames for terminated processes
 *   5. updateMetrics()                 — recompute MemoryMetrics
 */

#include "modules/memory/MemoryManager.h"
#include "modules/memory/IReplacementPolicy.h"
#include "modules/memory/PageTable.h"
#include "modules/memory/PageTableEntry.h"
#include "modules/memory/FrameTableEntry.h"
#include "modules/memory/MemoryMetrics.h"
#include "modules/memory/policies/FIFOPolicy.h"
#include "modules/memory/policies/LRUPolicy.h"
#include "modules/process/PCB.h"
#include "core/SimulationState.h"
#include "core/EventBus.h"
#include "core/SimEvent.h"
#include "core/DecisionLogEntry.h"

#include <algorithm>
#include <string>
#include <stdexcept>

// ═══════════════════════════════════════════════════════════════
// Constructor / ISimModule interface
// ═══════════════════════════════════════════════════════════════

MemoryManager::MemoryManager()
    : status_(ModuleStatus::IDLE)
    , activePolicy_(std::make_unique<FIFOPolicy>())
    , totalMemoryAccesses_(0)
    , roundRobinCounter_(0)
    , accessSequence_()
    , accessSequenceIdx_(0)
{}

std::string MemoryManager::getModuleName() const {
    return "MemoryManager";
}

ModuleStatus MemoryManager::getStatus() const {
    return status_;
}

void MemoryManager::reset() {
    status_ = ModuleStatus::IDLE;
    activePolicy_ = std::make_unique<FIFOPolicy>();
    totalMemoryAccesses_ = 0;
    roundRobinCounter_ = 0;
    accessSequence_.clear();
    accessSequenceIdx_ = 0;
}

// ═══════════════════════════════════════════════════════════════
// onTick() — Core per-tick logic
// ═══════════════════════════════════════════════════════════════

void MemoryManager::onTick(SimulationState& state, EventBus& bus) {
    // Step 1: Create page tables for newly admitted processes
    initPageTablesForNewProcesses(state, bus);

    // Step 2: Clear reference bits on all PTEs (per DataDictionary §7.1)
    clearReferenceBits(state);

    // Step 3: Simulate one memory access for the running process
    simulateMemoryAccess(state, bus);

    // Step 4: Free frames belonging to terminated processes
    cleanupTerminatedProcessPages(state, bus);

    // Step 5: Recompute memory metrics
    updateMetrics(state);

    // Mark module as active
    status_ = ModuleStatus::ACTIVE;
}

// ═══════════════════════════════════════════════════════════════
// initPageTablesForNewProcesses() — create page tables for admitted processes
// ═══════════════════════════════════════════════════════════════

void MemoryManager::initPageTablesForNewProcesses(SimulationState& state, EventBus& bus) {
    for (auto& [pid, pcb] : state.processTable) {
        // Process must be past NEW state and have a pageTableId assigned
        if (pcb.state == ProcessState::NEW || pcb.state == ProcessState::TERMINATED) {
            continue;
        }

        // Skip if page table already exists
        if (state.pageTables.find(pid) != state.pageTables.end()) {
            continue;
        }

        // Create page table for this process
        PageTable pt;
        pt.ownerPid = pid;
        pt.pageSize = 256;  // Default page size (configurable in future)

        // Create one PTE per virtual page, all initially invalid
        for (uint32_t vpn = 0; vpn < pcb.memoryRequirement; vpn++) {
            PageTableEntry pte;
            pte.virtualPageNumber = vpn;
            pte.frameNumber = -1;
            pte.valid = false;
            pte.dirty = false;
            pte.referenced = false;
            pte.loadTick = 0;
            pte.lastAccessTick = 0;
            pt.entries.push_back(pte);
        }

        state.pageTables[pid] = pt;

        // Publish event (consumed by API Bridge for UI display)
        bus.publish(SimEvent(
            state.currentTick,
            EventTypes::PROCESS_STATE_CHANGED,
            pid, pid, -1,
            "Page table created for " + pcb.name + " (PID " + std::to_string(pid)
                + "): " + std::to_string(pcb.memoryRequirement) + " virtual pages"
        ));

        // Log decision
        state.decisionLog.emplace_back(
            state.currentTick,
            "Memory Manager: Created page table for " + pcb.name
                + " (PID " + std::to_string(pid) + ") with "
                + std::to_string(pcb.memoryRequirement) + " pages"
        );
    }
}

// ═══════════════════════════════════════════════════════════════
// clearReferenceBits() — reset referenced flags each tick
// ═══════════════════════════════════════════════════════════════

void MemoryManager::clearReferenceBits(SimulationState& state) {
    for (auto& [pid, pt] : state.pageTables) {
        for (auto& pte : pt.entries) {
            pte.referenced = false;
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// simulateMemoryAccess() — access VPN for running process
// ═══════════════════════════════════════════════════════════════

void MemoryManager::simulateMemoryAccess(SimulationState& state, EventBus& bus) {
    // No running process — no memory access this tick
    if (state.runningPID == -1) return;

    auto pcbIt = state.processTable.find(state.runningPID);
    if (pcbIt == state.processTable.end()) return;

    PCB& pcb = pcbIt->second;

    // Find the process's page table
    auto ptIt = state.pageTables.find(state.runningPID);
    if (ptIt == state.pageTables.end()) return;

    PageTable& pt = ptIt->second;
    if (pt.entries.empty()) return;

    // Determine which VPN to access this tick
    uint32_t vpn = getNextVPN(pcb.memoryRequirement);

    // Safety check: VPN must be within bounds
    if (vpn >= static_cast<uint32_t>(pt.entries.size())) {
        vpn = vpn % static_cast<uint32_t>(pt.entries.size());
    }

    // Increment total memory access counter
    totalMemoryAccesses_++;

    PageTableEntry& pte = pt.entries[vpn];
    pte.referenced = true;

    if (pte.valid) {
        // ── Page HIT — update access timestamp ──────────────
        pte.lastAccessTick = state.currentTick;

        // Update frame table as well
        if (pte.frameNumber >= 0 &&
            pte.frameNumber < static_cast<int>(state.frameTable.size())) {
            state.frameTable[pte.frameNumber].lastAccessTick = state.currentTick;
        }
    } else {
        // ── Page FAULT ──────────────────────────────────────
        pcb.pageFaultCount++;
        state.memMetrics.totalPageFaults++;

        // Publish PAGE_FAULT event
        bus.publish(SimEvent(
            state.currentTick,
            EventTypes::PAGE_FAULT,
            state.runningPID, state.runningPID,
            static_cast<int>(vpn),
            "Page fault for " + pcb.name + " (PID " + std::to_string(state.runningPID)
                + ") accessing VPN " + std::to_string(vpn)
        ));

        // Log decision
        state.decisionLog.emplace_back(
            state.currentTick,
            "PAGE FAULT: " + pcb.name + " (PID " + std::to_string(state.runningPID)
                + ") VPN " + std::to_string(vpn) + " not in memory"
        );

        // Try to find a free frame
        int freeFrame = findFreeFrame(state);

        if (freeFrame >= 0) {
            // ── Free frame available — load directly ────────
            loadPage(state, bus, state.runningPID, vpn, freeFrame);

            state.decisionLog.emplace_back(
                state.currentTick,
                "Loaded VPN " + std::to_string(vpn) + " of PID "
                    + std::to_string(state.runningPID) + " into free frame "
                    + std::to_string(freeFrame)
            );
        } else {
            // ── All frames occupied — evict via policy ──────
            int victimFrame = activePolicy_->selectVictimFrame(state.frameTable);

            if (victimFrame < 0) return;  // Should not happen if frames exist

            // Record who we're evicting for the log
            FrameTableEntry& victimEntry = state.frameTable[victimFrame];
            int evictedPid = victimEntry.ownerPid;
            uint32_t evictedVpn = victimEntry.virtualPageNumber;

            evictPage(state, bus, victimFrame);
            loadPage(state, bus, state.runningPID, vpn, victimFrame);
            state.memMetrics.totalReplacements++;

            // Publish PAGE_REPLACED event
            bus.publish(SimEvent(
                state.currentTick,
                EventTypes::PAGE_REPLACED,
                state.runningPID, evictedPid,
                victimFrame,
                "Replaced frame " + std::to_string(victimFrame)
                    + ": evicted PID " + std::to_string(evictedPid)
                    + " VPN " + std::to_string(evictedVpn)
                    + ", loaded PID " + std::to_string(state.runningPID)
                    + " VPN " + std::to_string(vpn)
                    + " [" + activePolicy_->policyName() + "]"
            ));

            state.decisionLog.emplace_back(
                state.currentTick,
                "PAGE REPLACEMENT [" + activePolicy_->policyName() + "]: "
                    + "Evicted PID " + std::to_string(evictedPid)
                    + " VPN " + std::to_string(evictedVpn)
                    + " from frame " + std::to_string(victimFrame)
                    + ", loaded PID " + std::to_string(state.runningPID)
                    + " VPN " + std::to_string(vpn)
            );
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// cleanupTerminatedProcessPages() — free frames on process termination
// ═══════════════════════════════════════════════════════════════

void MemoryManager::cleanupTerminatedProcessPages(SimulationState& state, EventBus& bus) {
    // Collect PIDs to remove (can't modify map during iteration)
    std::vector<int> pidsToClean;

    for (auto& [pid, pcb] : state.processTable) {
        if (pcb.state == ProcessState::TERMINATED &&
            state.pageTables.find(pid) != state.pageTables.end()) {
            pidsToClean.push_back(pid);
        }
    }

    for (int pid : pidsToClean) {
        PageTable& pt = state.pageTables[pid];
        int framesFreed = 0;

        // Free all frames owned by this process
        for (auto& pte : pt.entries) {
            if (pte.valid && pte.frameNumber >= 0 &&
                pte.frameNumber < static_cast<int>(state.frameTable.size())) {
                FrameTableEntry& frame = state.frameTable[pte.frameNumber];
                frame.occupied = false;
                frame.ownerPid = -1;
                frame.virtualPageNumber = 0;
                frame.loadTick = 0;
                frame.lastAccessTick = 0;
                framesFreed++;
            }
        }

        // Remove page table
        state.pageTables.erase(pid);

        if (framesFreed > 0) {
            state.decisionLog.emplace_back(
                state.currentTick,
                "Memory Manager: Freed " + std::to_string(framesFreed)
                    + " frames from terminated PID " + std::to_string(pid)
            );
        }

        // Suppress unused parameter warning
        (void)bus;
    }
}

// ═══════════════════════════════════════════════════════════════
// updateMetrics() — Recompute MemoryMetrics
// ═══════════════════════════════════════════════════════════════

void MemoryManager::updateMetrics(SimulationState& state) {
    // Count occupied frames
    uint32_t occupied = 0;
    for (const auto& frame : state.frameTable) {
        if (frame.occupied) {
            occupied++;
        }
    }

    state.memMetrics.occupiedFrames = occupied;
    state.memMetrics.totalFrames = static_cast<uint32_t>(state.frameTable.size());
    state.memMetrics.activePolicy = activePolicy_->policyName();

    // Page fault rate: faults per 100 memory accesses
    if (totalMemoryAccesses_ > 0) {
        state.memMetrics.pageFaultRate =
            (static_cast<float>(state.memMetrics.totalPageFaults)
             / static_cast<float>(totalMemoryAccesses_)) * 100.0f;
    } else {
        state.memMetrics.pageFaultRate = 0.0f;
    }
}

// ═══════════════════════════════════════════════════════════════
// Policy Management
// ═══════════════════════════════════════════════════════════════

void MemoryManager::setPolicy(const std::string& policyName) {
    auto newPolicy = createPolicy(policyName);
    if (newPolicy) {
        activePolicy_ = std::move(newPolicy);
    }
}

std::string MemoryManager::getActivePolicyName() const {
    return activePolicy_ ? activePolicy_->policyName() : "NONE";
}

std::unique_ptr<IReplacementPolicy> MemoryManager::createPolicy(const std::string& name) {
    if (name == "FIFO") {
        return std::make_unique<FIFOPolicy>();
    } else if (name == "LRU") {
        return std::make_unique<LRUPolicy>();
    }
    // Unknown policy — return nullptr (caller should handle)
    return nullptr;
}

// ═══════════════════════════════════════════════════════════════
// Frame Table Initialization
// ═══════════════════════════════════════════════════════════════

void MemoryManager::initializeFrameTable(SimulationState& state, uint32_t totalFrames) {
    // Enforce minimum of 4 frames per DataDictionary §7.4
    if (totalFrames < 4) {
        totalFrames = 4;
    }

    state.frameTable.clear();
    state.frameTable.reserve(totalFrames);

    for (uint32_t i = 0; i < totalFrames; i++) {
        FrameTableEntry entry;
        entry.frameNumber = static_cast<int>(i);
        entry.occupied = false;
        entry.ownerPid = -1;
        entry.virtualPageNumber = 0;
        entry.loadTick = 0;
        entry.lastAccessTick = 0;
        state.frameTable.push_back(entry);
    }

    state.memMetrics.totalFrames = totalFrames;
    state.memMetrics.occupiedFrames = 0;
    state.memMetrics.totalPageFaults = 0;
    state.memMetrics.pageFaultRate = 0.0f;
    state.memMetrics.totalReplacements = 0;
    state.memMetrics.activePolicy = activePolicy_->policyName();
}

// ═══════════════════════════════════════════════════════════════
// Test Support — Explicit Access Sequence
// ═══════════════════════════════════════════════════════════════

void MemoryManager::setAccessSequence(const std::vector<uint32_t>& vpns) {
    accessSequence_ = vpns;
    accessSequenceIdx_ = 0;
}

// ═══════════════════════════════════════════════════════════════
// Helpers
// ═══════════════════════════════════════════════════════════════

int MemoryManager::findFreeFrame(const SimulationState& state) {
    for (const auto& frame : state.frameTable) {
        if (!frame.occupied) {
            return frame.frameNumber;
        }
    }
    return -1;  // All frames occupied
}

void MemoryManager::loadPage(SimulationState& state, EventBus& bus,
                             int pid, uint32_t vpn, int frameIdx) {
    // Update frame table entry
    FrameTableEntry& frame = state.frameTable[frameIdx];
    frame.occupied = true;
    frame.ownerPid = pid;
    frame.virtualPageNumber = vpn;
    frame.loadTick = state.currentTick;
    frame.lastAccessTick = state.currentTick;

    // Update page table entry
    auto ptIt = state.pageTables.find(pid);
    if (ptIt != state.pageTables.end() &&
        vpn < static_cast<uint32_t>(ptIt->second.entries.size())) {
        PageTableEntry& pte = ptIt->second.entries[vpn];
        pte.valid = true;
        pte.frameNumber = frameIdx;
        pte.loadTick = state.currentTick;
        pte.lastAccessTick = state.currentTick;
        pte.dirty = false;
        pte.referenced = true;
    }

    // Suppress unused parameter warning — bus reserved for future events
    (void)bus;
}

void MemoryManager::evictPage(SimulationState& state, EventBus& bus,
                              int victimFrame) {
    FrameTableEntry& frame = state.frameTable[victimFrame];
    int oldPid = frame.ownerPid;
    uint32_t oldVpn = frame.virtualPageNumber;

    // Invalidate the old page table entry
    auto ptIt = state.pageTables.find(oldPid);
    if (ptIt != state.pageTables.end() &&
        oldVpn < static_cast<uint32_t>(ptIt->second.entries.size())) {
        PageTableEntry& pte = ptIt->second.entries[oldVpn];
        pte.valid = false;
        pte.frameNumber = -1;
    }

    // Clear the frame
    frame.occupied = false;
    frame.ownerPid = -1;
    frame.virtualPageNumber = 0;
    frame.loadTick = 0;
    frame.lastAccessTick = 0;

    // Suppress unused parameter warning
    (void)bus;
}

uint32_t MemoryManager::getNextVPN(uint32_t memoryRequirement) {
    if (memoryRequirement == 0) return 0;

    // Check if we have an explicit access sequence
    if (!accessSequence_.empty() && accessSequenceIdx_ < accessSequence_.size()) {
        uint32_t vpn = accessSequence_[accessSequenceIdx_];
        accessSequenceIdx_++;
        return vpn;
    }

    // Default: round-robin scan through process's virtual pages
    uint32_t vpn = static_cast<uint32_t>(roundRobinCounter_ % memoryRequirement);
    roundRobinCounter_++;
    return vpn;
}
