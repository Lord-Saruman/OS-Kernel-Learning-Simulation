/**
 * RestServer.cpp — API Bridge Implementation
 *
 * Reference: SDD §5.2 (REST endpoints), §5.3 (WebSocket broadcast)
 *            DataDictionary §8.2 (ProcessSpec), §8.3 (WorkloadScenario)
 *            DataDictionary §9 (JSON naming: camelCase → snake_case)
 *
 * Implements 12 REST routes + 1 WebSocket route using Crow.
 * All on a single port. CORS enabled for localhost:5173 (Vite).
 */

#include "bridge/RestServer.h"
#include "bridge/StateSerializer.h"
#include "bridge/WorkloadLoader.h"

#include "core/SimulationState.h"
#include "core/EventBus.h"
#include "core/ClockController.h"
#include "core/SimEnums.h"
#include "modules/process/ProcessManager.h"
#include "modules/process/ProcessSpec.h"
#include "modules/scheduler/Scheduler.h"
#include "modules/memory/MemoryManager.h"
#include "modules/sync/SyncManager.h"

#include <crow.h>
#include <nlohmann/json.hpp>

#include <mutex>
#include <set>
#include <string>
#include <iostream>
#include <shared_mutex>

using json = nlohmann::json;

// ═════════════════════════════════════════════════════════════
// Pimpl implementation
// ═════════════════════════════════════════════════════════════

struct RestServer::Impl {
    SimulationState& state;
    EventBus& bus;
    ClockController& clock;
    ProcessManager& processManager;
    Scheduler& scheduler;
    MemoryManager& memoryManager;
    SyncManager& syncManager;

    crow::SimpleApp app;

    // WebSocket connections
    std::mutex wsMutex;
    std::set<crow::websocket::connection*> wsClients;

    // EventBus subscription ID for tick broadcast
    int tickSubId = -1;

    Impl(SimulationState& s, EventBus& b, ClockController& c,
         ProcessManager& pm, Scheduler& sch, MemoryManager& mm, SyncManager& sm)
        : state(s), bus(b), clock(c)
        , processManager(pm), scheduler(sch)
        , memoryManager(mm), syncManager(sm)
    {}

    // ── Helper: JSON error response ─────────────────────────
    static crow::response errorResponse(int code, const std::string& msg) {
        json j = {{"ok", false}, {"error", msg}};
        auto resp = crow::response(code, j.dump());
        resp.set_header("Content-Type", "application/json");
        return resp;
    }

    // ── Helper: JSON success response ───────────────────────
    static crow::response okResponse(const json& extra = json::object()) {
        json j = {{"ok", true}};
        j.merge_patch(extra);
        auto resp = crow::response(200, j.dump());
        resp.set_header("Content-Type", "application/json");
        return resp;
    }

    // ── Helper: Add CORS headers ────────────────────────────
    static void addCorsHeaders(crow::response& resp) {
        resp.set_header("Access-Control-Allow-Origin", "*");
        resp.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        resp.set_header("Access-Control-Allow-Headers", "Content-Type");
    }

    // ── Broadcast state to all WebSocket clients ────────────
    void broadcastState() {
        json snapshot;
        {
            std::shared_lock<std::shared_mutex> lock(state.stateMutex);
            snapshot = StateSerializer::serializeState(state, bus);
        }
        std::string payload = snapshot.dump();

        std::lock_guard<std::mutex> wsLock(wsMutex);
        for (auto* conn : wsClients) {
            conn->send_text(payload);
        }
    }

    // ── Register all routes ─────────────────────────────────
    void setupRoutes() {

        // ─── CORS preflight for all routes ──────────────────
        CROW_ROUTE(app, "/<path>").methods(crow::HTTPMethod::OPTIONS)
        ([](const crow::request&, const std::string&) {
            crow::response resp(204);
            Impl::addCorsHeaders(resp);
            return resp;
        });

        // ─── POST /sim/start ────────────────────────────────
        CROW_ROUTE(app, "/sim/start").methods(crow::HTTPMethod::POST)
        ([this](const crow::request&) {
            clock.start();
            broadcastState();
            auto resp = okResponse({{"status", toString(state.status)}});
            addCorsHeaders(resp);
            return resp;
        });

        // ─── POST /sim/pause ────────────────────────────────
        CROW_ROUTE(app, "/sim/pause").methods(crow::HTTPMethod::POST)
        ([this](const crow::request&) {
            clock.pause();
            broadcastState();
            auto resp = okResponse({{"status", toString(state.status)}});
            addCorsHeaders(resp);
            return resp;
        });

        // ─── POST /sim/reset ────────────────────────────────
        CROW_ROUTE(app, "/sim/reset").methods(crow::HTTPMethod::POST)
        ([this](const crow::request&) {
            clock.reset();
            broadcastState();
            auto resp = okResponse({{"status", toString(state.status)}});
            addCorsHeaders(resp);
            return resp;
        });

        // ─── POST /sim/step ─────────────────────────────────
        CROW_ROUTE(app, "/sim/step").methods(crow::HTTPMethod::POST)
        ([this](const crow::request&) {
            // Auto-start the engine if still IDLE (so Step works
            // without requiring the user to click Start first).
            if (state.status == SimStatus::IDLE) {
                clock.start();
            }

            const uint64_t prev = clock.getCompletedTick();
            bool accepted = clock.requestStep();
            if (accepted) {
                clock.waitForTickAdvance(prev, 2000);
            }
            broadcastState();
            auto resp = okResponse({
                {"accepted", accepted},
                {"tick", state.currentTick}
            });
            addCorsHeaders(resp);
            return resp;
        });

        // ─── POST /sim/mode ─────────────────────────────────
        CROW_ROUTE(app, "/sim/mode").methods(crow::HTTPMethod::POST)
        ([this](const crow::request& req) {
            try {
                auto body = json::parse(req.body);
                std::string modeStr = body.value("mode", "");
                SimMode mode = simModeFromString(modeStr);
                clock.setMode(mode);
                auto resp = okResponse({{"mode", toString(mode)}});
                addCorsHeaders(resp);
                return resp;
            } catch (const std::exception& e) {
                auto resp = errorResponse(400, e.what());
                addCorsHeaders(resp);
                return resp;
            }
        });

        // ─── POST /sim/speed ────────────────────────────────
        CROW_ROUTE(app, "/sim/speed").methods(crow::HTTPMethod::POST)
        ([this](const crow::request& req) {
            try {
                auto body = json::parse(req.body);
                uint32_t ms = body.value("ms", 500u);
                clock.setAutoSpeedMs(ms);
                auto resp = okResponse({{"auto_speed_ms", ms}});
                addCorsHeaders(resp);
                return resp;
            } catch (const std::exception& e) {
                auto resp = errorResponse(400, e.what());
                addCorsHeaders(resp);
                return resp;
            }
        });

        // ─── POST /process/create ───────────────────────────
        CROW_ROUTE(app, "/process/create").methods(crow::HTTPMethod::POST)
        ([this](const crow::request& req) {
            try {
                auto body = json::parse(req.body);

                ProcessSpec spec;
                spec.name = body.value("name", std::string(""));
                spec.type = processTypeFromString(
                    body.value("type", std::string("CPU_BOUND")));
                spec.priority = body.value("priority", 5);
                spec.cpuBurst = body.value("cpu_burst", 0u);
                spec.ioBurstDuration = body.value("io_burst_duration", 0u);
                spec.memoryRequirement = body.value("memory_requirement", 0u);

                int pid;
                {
                    std::unique_lock<std::shared_mutex> lock(state.stateMutex);
                    pid = processManager.createProcess(state, bus, spec);
                }
                broadcastState();
                auto resp = okResponse({{"pid", pid}});
                addCorsHeaders(resp);
                return resp;
            } catch (const std::exception& e) {
                auto resp = errorResponse(400, e.what());
                addCorsHeaders(resp);
                return resp;
            }
        });

        // ─── POST /process/kill ─────────────────────────────
        CROW_ROUTE(app, "/process/kill").methods(crow::HTTPMethod::POST)
        ([this](const crow::request& req) {
            try {
                auto body = json::parse(req.body);
                int pid = body.value("pid", -1);
                if (pid < 1) {
                    auto resp = errorResponse(400, "Invalid pid");
                    addCorsHeaders(resp);
                    return resp;
                }
                {
                    std::unique_lock<std::shared_mutex> lock(state.stateMutex);
                    processManager.killProcess(state, bus, pid);
                }
                broadcastState();
                auto resp = okResponse({{"killed_pid", pid}});
                addCorsHeaders(resp);
                return resp;
            } catch (const std::exception& e) {
                auto resp = errorResponse(400, e.what());
                addCorsHeaders(resp);
                return resp;
            }
        });

        // ─── POST /scheduler/policy ─────────────────────────
        CROW_ROUTE(app, "/scheduler/policy").methods(crow::HTTPMethod::POST)
        ([this](const crow::request& req) {
            try {
                auto body = json::parse(req.body);
                std::string policy = body.value("policy", std::string(""));
                {
                    std::unique_lock<std::shared_mutex> lock(state.stateMutex);
                    scheduler.setPolicy(policy);
                    state.activePolicy = scheduler.getActivePolicyName();
                }
                broadcastState();
                auto resp = okResponse({{"active_policy", state.activePolicy}});
                addCorsHeaders(resp);
                return resp;
            } catch (const std::exception& e) {
                auto resp = errorResponse(400, e.what());
                addCorsHeaders(resp);
                return resp;
            }
        });

        // ─── POST /scheduler/quantum ────────────────────────
        CROW_ROUTE(app, "/scheduler/quantum").methods(crow::HTTPMethod::POST)
        ([this](const crow::request& req) {
            try {
                auto body = json::parse(req.body);
                uint32_t quantum = body.value("quantum", 2u);
                clock.setTimeQuantum(quantum);
                auto resp = okResponse({{"time_quantum", state.timeQuantum}});
                addCorsHeaders(resp);
                return resp;
            } catch (const std::exception& e) {
                auto resp = errorResponse(400, e.what());
                addCorsHeaders(resp);
                return resp;
            }
        });

        // ─── POST /memory/policy ────────────────────────────
        CROW_ROUTE(app, "/memory/policy").methods(crow::HTTPMethod::POST)
        ([this](const crow::request& req) {
            try {
                auto body = json::parse(req.body);
                std::string policy = body.value("policy", std::string(""));
                {
                    std::unique_lock<std::shared_mutex> lock(state.stateMutex);
                    memoryManager.setPolicy(policy);
                    state.activeReplacement = memoryManager.getActivePolicyName();
                }
                broadcastState();
                auto resp = okResponse({
                    {"active_replacement", state.activeReplacement}
                });
                addCorsHeaders(resp);
                return resp;
            } catch (const std::exception& e) {
                auto resp = errorResponse(400, e.what());
                addCorsHeaders(resp);
                return resp;
            }
        });

        // ─── POST /memory/frames ────────────────────────────
        CROW_ROUTE(app, "/memory/frames").methods(crow::HTTPMethod::POST)
        ([this](const crow::request& req) {
            try {
                auto body = json::parse(req.body);
                uint32_t frameCount = body.value("frame_count", 16u);
                if (frameCount != 4u && frameCount != 6u &&
                    frameCount != 8u && frameCount != 16u) {
                    auto resp = errorResponse(400, "frame_count must be one of: 4, 6, 8, 16");
                    addCorsHeaders(resp);
                    return resp;
                }

                // Apply new runtime frame count and reset the simulation so
                // frame table + page tables remain consistent.
                clock.setFrameCount(frameCount);
                clock.reset();
                broadcastState();

                auto resp = okResponse({
                    {"frame_count", frameCount},
                    {"status", toString(state.status)}
                });
                addCorsHeaders(resp);
                return resp;
            } catch (const std::exception& e) {
                auto resp = errorResponse(400, e.what());
                addCorsHeaders(resp);
                return resp;
            }
        });

        // ─── POST /memory/access_sequence ────────────────────
        // Pushes an explicit virtual-page reference string into the
        // MemoryManager. Lets the API/dashboard drive deterministic
        // textbook scenarios (e.g., Silberschatz: FIFO=15, LRU=12).
        // Body: { "vpns": [7,0,1,2,0,3,...] }
        CROW_ROUTE(app, "/memory/access_sequence").methods(crow::HTTPMethod::POST)
        ([this](const crow::request& req) {
            try {
                auto body = json::parse(req.body);
                if (!body.contains("vpns") || !body["vpns"].is_array()) {
                    auto resp = errorResponse(400,
                        "Body must contain a 'vpns' array of non-negative integers");
                    addCorsHeaders(resp);
                    return resp;
                }
                std::vector<uint32_t> vpns;
                vpns.reserve(body["vpns"].size());
                for (const auto& v : body["vpns"]) {
                    if (!v.is_number_unsigned() && !(v.is_number_integer() && v.get<int64_t>() >= 0)) {
                        auto resp = errorResponse(400,
                            "All VPNs must be non-negative integers");
                        addCorsHeaders(resp);
                        return resp;
                    }
                    vpns.push_back(v.get<uint32_t>());
                }
                {
                    std::unique_lock<std::shared_mutex> lock(state.stateMutex);
                    memoryManager.setAccessSequence(vpns);
                }
                broadcastState();
                auto resp = okResponse({
                    {"sequence_length", static_cast<int>(vpns.size())}
                });
                addCorsHeaders(resp);
                return resp;
            } catch (const std::exception& e) {
                auto resp = errorResponse(400, e.what());
                addCorsHeaders(resp);
                return resp;
            }
        });

        // ─── POST /workload/load ────────────────────────────
        CROW_ROUTE(app, "/workload/load").methods(crow::HTTPMethod::POST)
        ([this](const crow::request& req) {
            try {
                auto body = json::parse(req.body);
                std::string scenario = body.value("scenario", std::string(""));
                auto specs = WorkloadLoader::loadScenario(scenario);

                std::vector<int> pids;
                {
                    std::unique_lock<std::shared_mutex> lock(state.stateMutex);
                    for (const auto& spec : specs) {
                        int pid = processManager.createProcess(state, bus, spec);
                        pids.push_back(pid);
                    }
                }

                broadcastState();
                auto resp = okResponse({
                    {"scenario", scenario},
                    {"process_count", static_cast<int>(pids.size())},
                    {"pids", pids}
                });
                addCorsHeaders(resp);
                return resp;
            } catch (const std::exception& e) {
                auto resp = errorResponse(400, e.what());
                addCorsHeaders(resp);
                return resp;
            }
        });

        // ─── POST /sync/demo ─────────────────────────────────
        CROW_ROUTE(app, "/sync/demo").methods(crow::HTTPMethod::POST)
        ([this](const crow::request& req) {
            try {
                auto body = json::parse(req.body);
                bool sync = body.value("sync", true);

                {
                    std::unique_lock<std::shared_mutex> lock(state.stateMutex);
                    syncManager.setupRaceConditionDemo(state, bus, sync);
                    
                    ProcessSpec writer1;
                    writer1.name = "DemoWriter_A";
                    writer1.type = ProcessType::CPU_BOUND;
                    writer1.priority = 5;
                    writer1.cpuBurst = 30;
                    processManager.createProcess(state, bus, writer1);

                    ProcessSpec writer2;
                    writer2.name = "DemoWriter_B";
                    writer2.type = ProcessType::CPU_BOUND;
                    writer2.priority = 5;
                    writer2.cpuBurst = 30;
                    processManager.createProcess(state, bus, writer2);
                }

                broadcastState();
                auto resp = okResponse({{"message", "Race condition demo started"}});
                addCorsHeaders(resp);
                return resp;
            } catch (const std::exception& e) {
                auto resp = errorResponse(400, e.what());
                addCorsHeaders(resp);
                return resp;
            }
        });

        // ─── GET /state/snapshot ────────────────────────────
        CROW_ROUTE(app, "/state/snapshot").methods(crow::HTTPMethod::GET)
        ([this](const crow::request&) {
            json snapshot;
            {
                std::shared_lock<std::shared_mutex> lock(state.stateMutex);
                snapshot = StateSerializer::serializeState(state, bus);
            }
            auto resp = crow::response(200, snapshot.dump());
            resp.set_header("Content-Type", "application/json");
            addCorsHeaders(resp);
            return resp;
        });

        // ─── WebSocket /ws ──────────────────────────────────
        CROW_WEBSOCKET_ROUTE(app, "/ws")
            .onopen([this](crow::websocket::connection& conn) {
                std::lock_guard<std::mutex> lock(wsMutex);
                wsClients.insert(&conn);
                std::cout << "[WS] Client connected ("
                          << wsClients.size() << " total)\n";

                // Send immediate state snapshot on connect
                json snapshot;
                {
                    std::shared_lock<std::shared_mutex> slock(state.stateMutex);
                    snapshot = StateSerializer::serializeState(state, bus);
                }
                conn.send_text(snapshot.dump());
            })
            .onclose([this](crow::websocket::connection& conn,
                            const std::string& /*reason*/) {
                std::lock_guard<std::mutex> lock(wsMutex);
                wsClients.erase(&conn);
                std::cout << "[WS] Client disconnected ("
                          << wsClients.size() << " total)\n";
            })
            .onmessage([](crow::websocket::connection&,
                          const std::string& /*data*/, bool) {
                // Frontend sends no messages via WS — commands go via REST
            });
    }

    // ── Subscribe to EventBus for tick broadcast ────────────
    void subscribeToTicks() {
        tickSubId = bus.subscribe(EventTypes::TICK_ADVANCED,
            [this](const SimEvent&) {
                broadcastState();
            });
    }
};

// ═════════════════════════════════════════════════════════════
// Public interface
// ═════════════════════════════════════════════════════════════

RestServer::RestServer(SimulationState& state,
                       EventBus& bus,
                       ClockController& clock,
                       ProcessManager& processManager,
                       Scheduler& scheduler,
                       MemoryManager& memoryManager,
                       SyncManager& syncManager)
    : impl_(std::make_unique<Impl>(state, bus, clock,
                                    processManager, scheduler,
                                    memoryManager, syncManager))
{
    impl_->setupRoutes();
    impl_->subscribeToTicks();
}

RestServer::~RestServer() {
    stop();
}

void RestServer::start(uint16_t port) {
    std::cout << "========================================\n";
    std::cout << "  Mini OS Kernel Simulator — API Bridge\n";
    std::cout << "  REST + WebSocket on port " << port << "\n";
    std::cout << "========================================\n\n";
    std::cout << "Endpoints:\n";
    std::cout << "  POST /sim/start|pause|reset|step|mode|speed\n";
    std::cout << "  POST /process/create|kill\n";
    std::cout << "  POST /scheduler/policy|quantum\n";
    std::cout << "  POST /memory/policy\n";
    std::cout << "  POST /memory/frames\n";
    std::cout << "  POST /workload/load\n";
    std::cout << "  GET  /state/snapshot\n";
    std::cout << "  WS   /ws\n\n";

    impl_->app.port(port)
              .multithreaded()
              .run();
}

void RestServer::stop() {
    impl_->app.stop();
}
