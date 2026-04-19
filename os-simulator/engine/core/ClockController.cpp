/**
 * ClockController.cpp - Simulation Clock Controller Implementation
 *
 * Phase 6 implementation:
 * - deterministic module phasing with worker threads + std::barrier
 * - STEP and AUTO runtime control
 * - safe pause/reset/shutdown semantics
 */

#include "core/ClockController.h"

#include <algorithm>
#include <barrier>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <thread>

#include "core/EventBus.h"
#include "core/SimEvent.h"
#include "core/SimulationState.h"
#include "modules/memory/MemoryManager.h"

using namespace std::chrono_literals;

class ClockController::Impl {
public:
    std::array<std::thread, ClockController::kModuleCount> workerThreads;
    std::thread clockThread;

    std::mutex controlMutex;
    std::condition_variable controlCv;
    std::condition_variable tickDoneCv;

    std::unique_ptr<std::barrier<>> phaseBarrier;
    ModuleSlot activeSlot = ModuleSlot::PROCESS;

    bool threadsStarted = false;
    bool shutdownRequested = false;
    bool stepRequested = false;
    bool tickInFlight = false;
    uint64_t completedTick = 0;
};

ClockController::ClockController(SimulationState& state, EventBus& bus)
    : state_(state)
    , bus_(bus)
    , modules_{}
    , memoryManager_(nullptr)
    , runtimeConfig_{}
    , impl_(std::make_unique<Impl>()) {}

ClockController::~ClockController() {
    shutdown();
}

size_t ClockController::slotIndex(ModuleSlot slot) {
    return static_cast<size_t>(slot);
}

void ClockController::registerModule(ModuleSlot slot, std::shared_ptr<ISimModule> module) {
    if (!module) {
        throw std::invalid_argument("registerModule: module cannot be null");
    }

    std::lock_guard<std::mutex> lock(impl_->controlMutex);
    if (impl_->threadsStarted) {
        throw std::runtime_error("registerModule: cannot register after start");
    }

    modules_[slotIndex(slot)] = std::move(module);

    if (slot == ModuleSlot::MEMORY) {
        memoryManager_ = std::dynamic_pointer_cast<MemoryManager>(modules_[slotIndex(slot)]);
    }
}

void ClockController::registerModule(std::shared_ptr<ISimModule> module) {
    if (!module) {
        throw std::invalid_argument("registerModule: module cannot be null");
    }

    std::lock_guard<std::mutex> lock(impl_->controlMutex);
    if (impl_->threadsStarted) {
        throw std::runtime_error("registerModule: cannot register after start");
    }

    auto it = std::find(modules_.begin(), modules_.end(), nullptr);
    if (it == modules_.end()) {
        throw std::runtime_error("registerModule: all 4 slots are already filled");
    }

    *it = std::move(module);
    size_t idx = static_cast<size_t>(std::distance(modules_.begin(), it));
    if (idx == slotIndex(ModuleSlot::MEMORY)) {
        memoryManager_ = std::dynamic_pointer_cast<MemoryManager>(*it);
    }
}

size_t ClockController::getModuleCount() const {
    return static_cast<size_t>(std::count_if(
        modules_.begin(),
        modules_.end(),
        [](const std::shared_ptr<ISimModule>& m) { return m != nullptr; }));
}

bool ClockController::allModulesRegistered() const {
    return std::all_of(
        modules_.begin(),
        modules_.end(),
        [](const std::shared_ptr<ISimModule>& m) { return m != nullptr; });
}

uint64_t ClockController::getCompletedTick() const {
    std::lock_guard<std::mutex> lock(impl_->controlMutex);
    return impl_->completedTick;
}

void ClockController::ensureThreadsStarted() {
    std::lock_guard<std::mutex> lock(impl_->controlMutex);

    if (impl_->threadsStarted) {
        return;
    }

    if (!allModulesRegistered()) {
        throw std::runtime_error("start: all 4 module slots must be registered");
    }

    impl_->shutdownRequested = false;
    impl_->stepRequested = false;
    impl_->tickInFlight = false;
    impl_->completedTick = state_.currentTick;

    // +1 participant for the controller thread.
    impl_->phaseBarrier = std::make_unique<std::barrier<>>(static_cast<ptrdiff_t>(kModuleCount + 1));

    for (size_t i = 0; i < kModuleCount; ++i) {
        ModuleSlot slot = static_cast<ModuleSlot>(i);
        impl_->workerThreads[i] = std::thread([this, slot]() { workerLoop(slot); });
    }

    impl_->clockThread = std::thread([this]() { clockLoop(); });
    impl_->threadsStarted = true;
}

void ClockController::start() {
    ensureThreadsStarted();

    std::lock_guard<std::mutex> lock(impl_->controlMutex);
    state_.status = SimStatus::RUNNING;
    impl_->controlCv.notify_all();
}

void ClockController::pause() {
    std::lock_guard<std::mutex> lock(impl_->controlMutex);
    if (state_.status == SimStatus::RUNNING) {
        state_.status = SimStatus::PAUSED;
        impl_->stepRequested = false;
    }
    impl_->controlCv.notify_all();
}

void ClockController::setMode(SimMode mode) {
    std::lock_guard<std::mutex> lock(impl_->controlMutex);
    state_.mode = mode;
    if (mode == SimMode::AUTO) {
        impl_->stepRequested = false;
    }
    impl_->controlCv.notify_all();
}

void ClockController::setAutoSpeedMs(uint32_t ms) {
    if (ms == 0) {
        ms = 1;
    }

    std::lock_guard<std::mutex> lock(impl_->controlMutex);
    state_.autoSpeedMs = ms;
    impl_->controlCv.notify_all();
}

bool ClockController::requestStep() {
    std::lock_guard<std::mutex> lock(impl_->controlMutex);

    if (impl_->shutdownRequested) {
        return false;
    }
    if (state_.mode != SimMode::STEP) {
        return false;
    }
    if (state_.status != SimStatus::RUNNING) {
        return false;
    }

    impl_->stepRequested = true;
    impl_->controlCv.notify_all();
    return true;
}

bool ClockController::waitForTickAdvance(uint64_t previousTick, uint32_t timeoutMs) {
    std::unique_lock<std::mutex> lock(impl_->controlMutex);
    return impl_->tickDoneCv.wait_for(
        lock,
        std::chrono::milliseconds(timeoutMs),
        [this, previousTick]() {
            return impl_->completedTick > previousTick || impl_->shutdownRequested;
        });
}

void ClockController::stepOnce() {
    const uint64_t prev = getCompletedTick();
    if (!requestStep()) {
        return;
    }
    (void)waitForTickAdvance(prev, 2000);
}

void ClockController::runPhase(ModuleSlot slot) {
    {
        std::lock_guard<std::mutex> lock(impl_->controlMutex);
        impl_->activeSlot = slot;
    }

    // Start phase.
    impl_->phaseBarrier->arrive_and_wait();
    // End phase.
    impl_->phaseBarrier->arrive_and_wait();
}

void ClockController::runSingleTick() {
    {
        std::lock_guard<std::mutex> lock(impl_->controlMutex);
        impl_->tickInFlight = true;
    }

    {
        std::unique_lock<std::shared_mutex> stateLock(state_.stateMutex);
        bus_.clearTickEvents();
        state_.currentTick++;
    }

    runPhase(ModuleSlot::PROCESS);
    runPhase(ModuleSlot::SCHEDULER);
    runPhase(ModuleSlot::MEMORY);
    runPhase(ModuleSlot::SYNC);

    uint64_t completedTick = 0;
    {
        std::shared_lock<std::shared_mutex> stateLock(state_.stateMutex);
        completedTick = state_.currentTick;
    }

    bus_.publish(SimEvent(
        completedTick,
        EventTypes::TICK_ADVANCED,
        -1,
        -1,
        -1,
        "Clock tick " + std::to_string(completedTick) + " completed"));

    {
        std::lock_guard<std::mutex> lock(impl_->controlMutex);
        impl_->completedTick = completedTick;
        impl_->tickInFlight = false;
    }
    impl_->tickDoneCv.notify_all();
}

void ClockController::releaseWorkersForShutdown() {
    if (!impl_->phaseBarrier) {
        return;
    }

    // Let workers pass one start/end phase cycle and observe shutdownRequested.
    impl_->phaseBarrier->arrive_and_wait();
    impl_->phaseBarrier->arrive_and_wait();
}

void ClockController::workerLoop(ModuleSlot slot) {
    for (;;) {
        // Wait for start of phase.
        impl_->phaseBarrier->arrive_and_wait();

        bool shouldStop = false;
        bool shouldRun = false;
        std::shared_ptr<ISimModule> module;
        {
            std::lock_guard<std::mutex> lock(impl_->controlMutex);
            shouldStop = impl_->shutdownRequested;
            shouldRun = (impl_->activeSlot == slot);
            module = modules_[slotIndex(slot)];
        }

        if (shouldRun && !shouldStop && module) {
            std::unique_lock<std::shared_mutex> stateLock(state_.stateMutex);
            module->onTick(state_, bus_);
        }

        // Wait for phase completion.
        impl_->phaseBarrier->arrive_and_wait();

        if (shouldStop) {
            break;
        }
    }
}

void ClockController::clockLoop() {
    for (;;) {
        std::unique_lock<std::mutex> lock(impl_->controlMutex);
        impl_->controlCv.wait(lock, [this]() {
            return impl_->shutdownRequested || state_.status == SimStatus::RUNNING;
        });

        if (impl_->shutdownRequested) {
            lock.unlock();
            releaseWorkersForShutdown();
            break;
        }

        const SimMode mode = state_.mode;
        const uint32_t speedMs = state_.autoSpeedMs;

        if (mode == SimMode::STEP) {
            impl_->controlCv.wait(lock, [this]() {
                return impl_->shutdownRequested
                    || (state_.status == SimStatus::RUNNING && impl_->stepRequested)
                    || state_.status != SimStatus::RUNNING
                    || state_.mode != SimMode::STEP;
            });

            if (impl_->shutdownRequested) {
                lock.unlock();
                releaseWorkersForShutdown();
                break;
            }

            if (state_.status != SimStatus::RUNNING || state_.mode != SimMode::STEP) {
                continue;
            }

            impl_->stepRequested = false;
            lock.unlock();
            runSingleTick();
            continue;
        }

        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(speedMs));

        lock.lock();
        if (impl_->shutdownRequested) {
            lock.unlock();
            releaseWorkersForShutdown();
            break;
        }
        if (state_.status != SimStatus::RUNNING || state_.mode != SimMode::AUTO) {
            continue;
        }
        lock.unlock();
        runSingleTick();
    }
}

void ClockController::reset() {
    ensureThreadsStarted();

    {
        std::unique_lock<std::mutex> lock(impl_->controlMutex);
        state_.status = SimStatus::PAUSED;
        impl_->stepRequested = false;
        impl_->controlCv.notify_all();
        impl_->tickDoneCv.wait(lock, [this]() { return !impl_->tickInFlight; });
    }

    {
        std::unique_lock<std::shared_mutex> stateLock(state_.stateMutex);
        state_.reset();
    }

    for (auto& module : modules_) {
        if (module) {
            module->reset();
        }
    }

    // Re-apply runtime bootstrap config (frame table must exist after reset).
    if (memoryManager_) {
        std::unique_lock<std::shared_mutex> stateLock(state_.stateMutex);
        memoryManager_->initializeFrameTable(state_, runtimeConfig_.frameCount);
    }

    bus_.clearTickEvents();

    {
        std::lock_guard<std::mutex> lock(impl_->controlMutex);
        state_.status = SimStatus::IDLE;
        impl_->completedTick = state_.currentTick;
    }
}

void ClockController::shutdown() {
    {
        std::lock_guard<std::mutex> lock(impl_->controlMutex);
        if (!impl_->threadsStarted) {
            return;
        }
        if (impl_->shutdownRequested) {
            // Already shutting down.
        } else {
            impl_->shutdownRequested = true;
            impl_->stepRequested = true;
        }
    }

    impl_->controlCv.notify_all();

    if (impl_->clockThread.joinable()) {
        impl_->clockThread.join();
    }

    for (auto& t : impl_->workerThreads) {
        if (t.joinable()) {
            t.join();
        }
    }

    std::lock_guard<std::mutex> lock(impl_->controlMutex);
    impl_->threadsStarted = false;
    impl_->phaseBarrier.reset();
}
