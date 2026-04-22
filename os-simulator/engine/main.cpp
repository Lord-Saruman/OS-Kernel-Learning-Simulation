/**
 * main.cpp - Mini OS Kernel Simulator Entry Point
 *
 * Phase 7: The engine now runs as a headless HTTP + WebSocket server.
 * All simulation control is via REST endpoints. The React dashboard
 * connects via WebSocket to receive live state updates.
 *
 * Previous Phase 6 console demo is preserved in git history.
 */

#include <csignal>
#include <iostream>
#include <memory>

#include "core/ClockController.h"
#include "core/EventBus.h"
#include "core/SimEnums.h"
#include "core/SimulationState.h"
#include "modules/memory/MemoryManager.h"
#include "modules/process/ProcessManager.h"
#include "modules/scheduler/Scheduler.h"
#include "modules/sync/SyncManager.h"
#include "bridge/RestServer.h"

// Global pointer for signal handler cleanup
static ClockController* g_clock = nullptr;
static RestServer* g_server = nullptr;

void signalHandler(int /*signum*/) {
    std::cout << "\n[Signal] Shutting down...\n";
    if (g_server) g_server->stop();
    if (g_clock)  g_clock->shutdown();
}

int main() {
    // ── Register signal handlers for clean shutdown ──────────
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // ── Core components ─────────────────────────────────────
    SimulationState state;
    EventBus bus;
    ClockController clock(state, bus);
    g_clock = &clock;

    // ── Create and register all 4 OS modules ────────────────
    auto processManager = std::make_shared<ProcessManager>();
    auto scheduler       = std::make_shared<Scheduler>();
    auto memoryManager   = std::make_shared<MemoryManager>();
    auto syncManager     = std::make_shared<SyncManager>();

    clock.registerModule(ModuleSlot::PROCESS,   processManager);
    clock.registerModule(ModuleSlot::SCHEDULER, scheduler);
    clock.registerModule(ModuleSlot::MEMORY,    memoryManager);
    clock.registerModule(ModuleSlot::SYNC,      syncManager);

    // Bootstrap frame table (also re-applied by clock.reset()).
    memoryManager->initializeFrameTable(state, 16);

    std::cout << "Registered modules: " << clock.getModuleCount() << "\n";
    std::cout << "Initial mode: " << toString(state.mode) << "\n";
    std::cout << "Initial status: " << toString(state.status) << "\n\n";

    // ── Start API Bridge (Phase 7) ──────────────────────────
    // This blocks — the simulation is now controlled via REST.
    // WebSocket broadcasts state after every tick.
    RestServer server(state, bus, clock,
                      *processManager, *scheduler,
                      *memoryManager, *syncManager);
    g_server = &server;

    server.start(8080);  // Blocking call

    // ── Cleanup (reached after server.stop()) ───────────────
    clock.shutdown();
    g_clock = nullptr;
    g_server = nullptr;

    std::cout << "\nEngine shut down cleanly.\n";
    return 0;
}
