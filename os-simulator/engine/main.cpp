/**
 * main.cpp — Mini OS Kernel Simulator Entry Point
 *
 * Phase 4: Registers ProcessManager, Scheduler, and MemoryManager modules.
 * Creates sample processes and demonstrates process lifecycle,
 * CPU scheduling, and memory management with page fault handling.
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

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Mini OS Kernel Simulator Engine v1.0  " << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Initialise core components
    SimulationState state;
    EventBus bus;
    ClockController clock(state, bus);

    // ── Register OS modules (order matters: ProcMgr → Scheduler → MemMgr) ──
    auto procMgr = std::make_shared<ProcessManager>();
    clock.registerModule(procMgr);

    // ── Phase 4: Register Memory Manager ────────────────────
    auto memMgr = std::make_shared<MemoryManager>();
    memMgr->initializeFrameTable(state, 16);  // 16 frames (customizable)
    clock.registerModule(memMgr);

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
    (void)pid2;  // Used implicitly via processTable iteration below

    ProcessSpec mixedSpec;
    mixedSpec.name = "mixed_task";
    mixedSpec.type = ProcessType::MIXED;
    mixedSpec.priority = 7;
    mixedSpec.cpuBurst = 6;
    mixedSpec.ioBurstDuration = 2;
    mixedSpec.memoryRequirement = 3;
    int pid3 = procMgr->createProcess(state, bus, mixedSpec);

    std::cout << std::endl;
    std::cout << "Processes created: " << state.processTable.size() << std::endl;
    std::cout << "Ready queue size:  " << state.readyQueue.size() << " (should be 0 — not yet admitted)" << std::endl;
    std::cout << std::endl;

    // ── Start simulation and step ───────────────────────────
    std::cout << "--- Starting simulation (STEP mode) ---" << std::endl;
    clock.start();

    // Tick 1: All NEW→READY, waiting time starts
    std::cout << "\n--- Tick 1 ---" << std::endl;
    clock.stepOnce();
    std::cout << "  Ready queue size: " << state.readyQueue.size() << std::endl;
    for (auto& [pid, pcb] : state.processTable) {
        std::cout << "  PID " << pid << " (" << pcb.name << "): "
                  << toString(pcb.state) << ", waitingTime=" << pcb.waitingTime << std::endl;
    }

    // Tick 2: Waiting time accumulates
    std::cout << "\n--- Tick 2 ---" << std::endl;
    clock.stepOnce();
    for (auto& [pid, pcb] : state.processTable) {
        std::cout << "  PID " << pid << " (" << pcb.name << "): "
                  << toString(pcb.state) << ", waitingTime=" << pcb.waitingTime << std::endl;
    }

    // ── Demonstrate kill ─────────────────────────────────────
    std::cout << "\n--- Killing process PID " << pid3 << " ---" << std::endl;
    procMgr->killProcess(state, bus, pid3);
    std::cout << "  PID " << pid3 << " state: "
              << toString(state.processTable[pid3].state) << std::endl;
    std::cout << "  Turnaround time: " << state.processTable[pid3].turnaroundTime << std::endl;
    std::cout << "  Ready queue size: " << state.readyQueue.size() << std::endl;

    // ── Demonstrate thread creation ──────────────────────────
    std::cout << "\n--- Creating thread for PID " << pid1 << " ---" << std::endl;
    int tid = procMgr->createThread(state, bus, pid1, 3, 2);
    std::cout << "  Thread TID: " << tid << std::endl;
    std::cout << "  Parent PID: " << state.threadTable[tid].parentPid << std::endl;
    std::cout << "  Thread state: " << toString(state.threadTable[tid].state) << std::endl;

    // ── Reset ────────────────────────────────────────────────
    std::cout << "\n--- Reset ---" << std::endl;
    clock.reset();
    std::cout << "  Status:     " << toString(state.status) << std::endl;
    std::cout << "  Tick:       " << state.currentTick << std::endl;
    std::cout << "  Processes:  " << state.processTable.size() << std::endl;
    std::cout << "  Ready queue: " << state.readyQueue.size() << std::endl;

    std::cout << std::endl;
    std::cout << "Phase 4 complete. ProcessManager + MemoryManager registered and operational." << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
