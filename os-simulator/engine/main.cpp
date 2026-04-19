/**
 * main.cpp - Mini OS Kernel Simulator Entry Point
 *
 * Phase 6 integration harness:
 * - registers all 4 subsystem modules
 * - demonstrates STEP mode
 * - switches to AUTO mode
 * - pauses and resets
 */

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

#include "core/ClockController.h"
#include "core/EventBus.h"
#include "core/SimEnums.h"
#include "core/SimulationState.h"
#include "modules/memory/MemoryManager.h"
#include "modules/process/ProcessManager.h"
#include "modules/process/ProcessSpec.h"
#include "modules/scheduler/Scheduler.h"
#include "modules/sync/SyncManager.h"

using namespace std::chrono_literals;

int main() {
    std::cout << "========================================\n";
    std::cout << "  Mini OS Kernel Simulator Engine v1.0\n";
    std::cout << "========================================\n\n";

    SimulationState state;
    EventBus bus;
    ClockController clock(state, bus);

    auto processManager = std::make_shared<ProcessManager>();
    auto scheduler = std::make_shared<Scheduler>();
    auto memoryManager = std::make_shared<MemoryManager>();
    auto syncManager = std::make_shared<SyncManager>();

    clock.registerModule(ModuleSlot::PROCESS, processManager);
    clock.registerModule(ModuleSlot::SCHEDULER, scheduler);
    clock.registerModule(ModuleSlot::MEMORY, memoryManager);
    clock.registerModule(ModuleSlot::SYNC, syncManager);

    // Bootstrap frame table (also re-applied by clock.reset()).
    memoryManager->initializeFrameTable(state, 16);

    // Optional console event stream.
    bus.subscribe(EventTypes::TICK_ADVANCED, [](const SimEvent& e) {
        std::cout << "[Tick] " << e.description << "\n";
    });

    ProcessSpec cpuSpec;
    cpuSpec.name = "cpu_worker";
    cpuSpec.type = ProcessType::CPU_BOUND;
    cpuSpec.priority = 3;
    cpuSpec.cpuBurst = 8;
    cpuSpec.memoryRequirement = 3;
    processManager->createProcess(state, bus, cpuSpec);

    ProcessSpec ioSpec;
    ioSpec.name = "io_worker";
    ioSpec.type = ProcessType::IO_BOUND;
    ioSpec.priority = 5;
    ioSpec.cpuBurst = 6;
    ioSpec.ioBurstDuration = 2;
    ioSpec.memoryRequirement = 2;
    processManager->createProcess(state, bus, ioSpec);

    std::cout << "Registered modules: " << clock.getModuleCount() << "\n";
    std::cout << "Initial mode: " << toString(state.mode) << "\n";
    std::cout << "Initial status: " << toString(state.status) << "\n\n";

    clock.start();

    std::cout << "--- STEP mode (3 ticks) ---\n";
    for (int i = 0; i < 3; ++i) {
        const uint64_t prevTick = clock.getCompletedTick();
        if (clock.requestStep()) {
            clock.waitForTickAdvance(prevTick, 2000);
            std::cout << "Tick " << state.currentTick
                      << " runningPID=" << state.runningPID
                      << " readyQueueSize=" << state.readyQueue.size() << "\n";
        }
    }

    std::cout << "\n--- Switch to AUTO mode ---\n";
    clock.setMode(SimMode::AUTO);
    clock.setAutoSpeedMs(120);

    const uint64_t beforeAuto = clock.getCompletedTick();
    std::this_thread::sleep_for(700ms);
    const uint64_t afterAuto = clock.getCompletedTick();
    std::cout << "AUTO advanced ticks by: " << (afterAuto - beforeAuto) << "\n";

    std::cout << "\n--- Pause ---\n";
    clock.pause();
    const uint64_t pausedTick = clock.getCompletedTick();
    std::this_thread::sleep_for(300ms);
    std::cout << "Tick stable after pause: "
              << (clock.getCompletedTick() == pausedTick ? "yes" : "no") << "\n";

    std::cout << "\n--- Reset ---\n";
    clock.reset();
    std::cout << "Status: " << toString(state.status) << "\n";
    std::cout << "Tick: " << state.currentTick << "\n";
    std::cout << "Processes: " << state.processTable.size() << "\n";
    std::cout << "Frame table size: " << state.frameTable.size() << "\n";

    clock.shutdown();

    std::cout << "\nPhase 6 integration runtime ready.\n";
    std::cout << "========================================\n";
    return 0;
}
