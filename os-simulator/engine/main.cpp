/**
 * main.cpp — Mini OS Kernel Simulator Entry Point
 *
 * Phase 5: Registers ProcessManager, Scheduler, MemoryManager, and SyncManager.
 * Creates sample processes and demonstrates process lifecycle,
 * CPU scheduling, memory management, and synchronization primitives.
 */

#include <iostream>
#include <string>
#include <memory>

#include "core/SimulationState.h"
#include "core/EventBus.h"
#include "core/ClockController.h"
#include "core/SimEnums.h"
#include "modules/process/ProcessManager.h"
#include "modules/process/ProcessSpec.h"
#include "modules/memory/MemoryManager.h"
#include "modules/sync/SyncManager.h"

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Mini OS Kernel Simulator Engine v1.0  " << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Initialise core components
    SimulationState state;
    EventBus bus;
    ClockController clock(state, bus);

    // ── Register OS modules (order matters: ProcMgr → Scheduler → MemMgr → SyncMgr) ──
    auto procMgr = std::make_shared<ProcessManager>();
    clock.registerModule(procMgr);

    // ── Phase 4: Register Memory Manager ────────────────────
    auto memMgr = std::make_shared<MemoryManager>();
    memMgr->initializeFrameTable(state, 16);  // 16 frames (customizable)
    clock.registerModule(memMgr);

    // ── Phase 5: Register Sync Manager ──────────────────────
    auto syncMgr = std::make_shared<SyncManager>();
    clock.registerModule(syncMgr);

    // Print initial state
    std::cout << "Status:       " << toString(state.status) << std::endl;
    std::cout << "Mode:         " << toString(state.mode) << std::endl;
    std::cout << "Tick:         " << state.currentTick << std::endl;
    std::cout << "Policy:       " << state.activePolicy << std::endl;
    std::cout << "Quantum:      " << state.timeQuantum << std::endl;
    std::cout << "Replacement:  " << state.activeReplacement << std::endl;
    std::cout << "Next PID:     " << state.nextPID << std::endl;
    std::cout << "Running PID:  " << state.runningPID << std::endl;
    std::cout << "Modules:      " << clock.getModuleCount() << std::endl;
    std::cout << std::endl;

    // Subscribe to events for demonstration
    bus.subscribe(EventTypes::PROCESS_CREATED, [](const SimEvent& e) {
        std::cout << "  [Event] " << e.eventType << ": " << e.description << std::endl;
    });
    bus.subscribe(EventTypes::PROCESS_STATE_CHANGED, [](const SimEvent& e) {
        std::cout << "  [Event] " << e.eventType << ": " << e.description << std::endl;
    });
    bus.subscribe(EventTypes::PROCESS_TERMINATED, [](const SimEvent& e) {
        std::cout << "  [Event] " << e.eventType << ": " << e.description << std::endl;
    });
    bus.subscribe(EventTypes::TICK_ADVANCED, [](const SimEvent& e) {
        std::cout << "  [Event] " << e.eventType << ": " << e.description << std::endl;
    });
    bus.subscribe(EventTypes::PAGE_FAULT, [](const SimEvent& e) {
        std::cout << "  [Event] " << e.eventType << ": " << e.description << std::endl;
    });
    bus.subscribe(EventTypes::PAGE_REPLACED, [](const SimEvent& e) {
        std::cout << "  [Event] " << e.eventType << ": " << e.description << std::endl;
    });
    bus.subscribe(EventTypes::LOCK_ACQUIRED, [](const SimEvent& e) {
        std::cout << "  [Event] " << e.eventType << ": " << e.description << std::endl;
    });
    bus.subscribe(EventTypes::LOCK_RELEASED, [](const SimEvent& e) {
        std::cout << "  [Event] " << e.eventType << ": " << e.description << std::endl;
    });
    bus.subscribe(EventTypes::PROCESS_BLOCKED, [](const SimEvent& e) {
        std::cout << "  [Event] " << e.eventType << ": " << e.description << std::endl;
    });
    bus.subscribe(EventTypes::PROCESS_UNBLOCKED, [](const SimEvent& e) {
        std::cout << "  [Event] " << e.eventType << ": " << e.description << std::endl;
    });

    // ── Create sample processes ─────────────────────────────
    std::cout << "--- Creating processes ---" << std::endl;

    ProcessSpec cpuSpec;
    cpuSpec.name = "cpu_worker";
    cpuSpec.type = ProcessType::CPU_BOUND;
    cpuSpec.priority = 3;
    cpuSpec.cpuBurst = 8;
    cpuSpec.memoryRequirement = 2;
    int pid1 = procMgr->createProcess(state, bus, cpuSpec);

    ProcessSpec ioSpec;
    ioSpec.name = "io_handler";
    ioSpec.type = ProcessType::IO_BOUND;
    ioSpec.priority = 5;
    ioSpec.cpuBurst = 4;
    ioSpec.ioBurstDuration = 3;
    ioSpec.memoryRequirement = 2;
    int pid2 = procMgr->createProcess(state, bus, ioSpec);

    std::cout << std::endl;
    std::cout << "Processes created: " << state.processTable.size() << std::endl;
    std::cout << std::endl;

    // ── Start simulation and step ───────────────────────────
    std::cout << "--- Starting simulation (STEP mode) ---" << std::endl;
    clock.start();

    // Tick 1: All NEW→READY
    std::cout << "\n--- Tick 1 ---" << std::endl;
    clock.stepOnce();
    std::cout << "  Ready queue size: " << state.readyQueue.size() << std::endl;
    for (auto& [pid, pcb] : state.processTable) {
        std::cout << "  PID " << pid << " (" << pcb.name << "): "
                  << toString(pcb.state) << std::endl;
    }

    // ── Phase 5: Sync Manager Demo ──────────────────────────
    std::cout << "\n--- Sync Manager Demo ---" << std::endl;

    // Create a mutex
    int mtxId = syncMgr->createMutex(state, bus, "shared_resource");
    std::cout << "  Created mutex 'shared_resource' (ID " << mtxId << ")" << std::endl;

    // Create a binary semaphore
    int semId = syncMgr->createSemaphore(state, bus, "buffer_sem",
                                          SyncPrimitiveType::SEMAPHORE_BINARY, 1);
    std::cout << "  Created semaphore 'buffer_sem' (ID " << semId << ")" << std::endl;

    // pid1 acquires the mutex
    std::cout << "\n--- PID " << pid1 << " acquiring mutex ---" << std::endl;
    syncMgr->requestAcquire(state, pid1, mtxId);
    clock.stepOnce();

    std::cout << "  Mutex locked: " << (state.mutexTable[mtxId].locked ? "yes" : "no")
              << std::endl;
    std::cout << "  Owner PID: " << state.mutexTable[mtxId].ownerPid << std::endl;

    // pid2 tries to acquire → should be blocked
    std::cout << "\n--- PID " << pid2 << " attempting acquire (should block) ---" << std::endl;
    syncMgr->requestAcquire(state, pid2, mtxId);
    clock.stepOnce();

    std::cout << "  PID " << pid2 << " state: "
              << toString(state.processTable[pid2].state) << std::endl;
    std::cout << "  Mutex waiters: " << state.mutexTable[mtxId].waitingPids.size()
              << std::endl;

    // pid1 releases → pid2 should be unblocked and acquire
    std::cout << "\n--- PID " << pid1 << " releasing mutex ---" << std::endl;
    syncMgr->requestRelease(state, pid1, mtxId);
    clock.stepOnce();

    std::cout << "  Mutex owner PID: " << state.mutexTable[mtxId].ownerPid << std::endl;
    std::cout << "  PID " << pid2 << " state: "
              << toString(state.processTable[pid2].state) << std::endl;

    // ── Reset ────────────────────────────────────────────────
    std::cout << "\n--- Reset ---" << std::endl;
    clock.reset();
    std::cout << "  Status:     " << toString(state.status) << std::endl;
    std::cout << "  Tick:       " << state.currentTick << std::endl;
    std::cout << "  Processes:  " << state.processTable.size() << std::endl;
    std::cout << "  Mutexes:    " << state.mutexTable.size() << std::endl;
    std::cout << "  Semaphores: " << state.semaphoreTable.size() << std::endl;

    std::cout << std::endl;
    std::cout << "Phase 5 complete. All 4 OS modules registered and operational." << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
