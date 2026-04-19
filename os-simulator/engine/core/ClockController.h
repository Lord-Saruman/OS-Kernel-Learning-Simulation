#pragma once

/**
 * ClockController.h - Simulation Clock Controller
 *
 * Reference: SDD Section 3.2
 *
 * Phase 6 runtime model:
 * - one dedicated clock thread
 * - one worker thread per module
 * - std::barrier synchronization between clock and workers
 * - deterministic phase order per tick:
 *   PROCESS -> SCHEDULER -> MEMORY -> SYNC
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "core/ISimModule.h"
#include "core/SimEnums.h"

// Forward declarations
struct SimulationState;
class EventBus;
class MemoryManager;

enum class ModuleSlot {
    PROCESS = 0,
    SCHEDULER = 1,
    MEMORY = 2,
    SYNC = 3
};

class ClockController {
public:
    ClockController(SimulationState& state, EventBus& bus);
    ~ClockController();

    // Explicit slot registration (preferred).
    void registerModule(ModuleSlot slot, std::shared_ptr<ISimModule> module);

    // Legacy helper: fills next empty slot in fixed phase order.
    void registerModule(std::shared_ptr<ISimModule> module);

    void start();
    void pause();
    void reset();
    void shutdown();

    void setMode(SimMode mode);
    void setAutoSpeedMs(uint32_t ms);

    // Request one step tick while in STEP mode and RUNNING state.
    bool requestStep();

    // Wait until completedTick_ advances beyond previousTick.
    bool waitForTickAdvance(uint64_t previousTick, uint32_t timeoutMs);

    // Legacy helper for single-step flows.
    void stepOnce();

    size_t getModuleCount() const;
    bool allModulesRegistered() const;
    uint64_t getCompletedTick() const;

private:
    static constexpr size_t kModuleCount = 4;

    struct RuntimeConfig {
        uint32_t frameCount = 16;
    };

    SimulationState& state_;
    EventBus& bus_;

    std::array<std::shared_ptr<ISimModule>, kModuleCount> modules_;
    std::shared_ptr<MemoryManager> memoryManager_;
    RuntimeConfig runtimeConfig_;

    class Impl;
    std::unique_ptr<Impl> impl_;

    void ensureThreadsStarted();
    void clockLoop();
    void workerLoop(ModuleSlot slot);
    void runSingleTick();
    void runPhase(ModuleSlot slot);
    void releaseWorkersForShutdown();

    static size_t slotIndex(ModuleSlot slot);
};
