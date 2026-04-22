#pragma once

/**
 * RestServer.h — API Bridge: REST + WebSocket Server
 *
 * Reference: SDD §5 (API Bridge Layer)
 *            SDD §5.2 (REST API — 12 Command Endpoints)
 *            SDD §5.3 (WebSocket — State Stream)
 *
 * The RestServer is a thin C++ server that bridges the simulation
 * engine and the React frontend. It owns NO simulation logic.
 *
 * Uses Crow (lightweight C++ HTTP framework) for both REST endpoints
 * and WebSocket connections on a single port.
 */

#include <memory>
#include <cstdint>

// Forward declarations — avoid pulling heavy Crow headers into every TU
struct SimulationState;
class EventBus;
class ClockController;
class ProcessManager;
class Scheduler;
class MemoryManager;
class SyncManager;

class RestServer {
public:
    /**
     * Construct the server with references to all engine components.
     * No network activity until start() is called.
     */
    RestServer(SimulationState& state,
               EventBus& bus,
               ClockController& clock,
               ProcessManager& processManager,
               Scheduler& scheduler,
               MemoryManager& memoryManager,
               SyncManager& syncManager);

    ~RestServer();

    /**
     * Start the HTTP + WebSocket server on the given port.
     * This call BLOCKS (runs the Crow event loop).
     * Call from the main thread after all modules are registered.
     *
     * @param port  TCP port to listen on (default 8080, per SDD §9.1)
     */
    void start(uint16_t port = 8080);

    /**
     * Request graceful shutdown of the server.
     * Safe to call from a signal handler or another thread.
     */
    void stop();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
