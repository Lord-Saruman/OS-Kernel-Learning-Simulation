#pragma once

/**
 * ISimModule.h — Abstract Base Class for OS Subsystem Modules
 *
 * Reference: SDD Section 3.3 — Module Interface Contract
 *
 * Every OS module must implement this interface. This is enforced via
 * C++ abstract base class. Any new module added in the future must
 * conform to this same interface — this is what makes the system extensible.
 *
 * The onTick() method is called once per clock tick. All module logic
 * lives inside this method. Modules read from and write to the shared
 * SimulationState object. They never call other modules.
 */

#include <string>

// Forward declarations to avoid circular includes
struct SimulationState;
class EventBus;

/**
 * ModuleStatus — Health/lifecycle status of a module
 */
enum class ModuleStatus {
    IDLE,    // Module initialised but simulation not started
    ACTIVE,  // Module processing ticks normally
    ERROR    // Module encountered an error
};

inline std::string toString(ModuleStatus s) {
    switch (s) {
        case ModuleStatus::IDLE:   return "IDLE";
        case ModuleStatus::ACTIVE: return "ACTIVE";
        case ModuleStatus::ERROR:  return "ERROR";
    }
    return "UNKNOWN";
}

/**
 * ISimModule — The interface that every OS module must implement
 */
class ISimModule {
public:
    /**
     * Called once per clock tick. All module logic lives here.
     * @param state  Shared simulation state (read/write with scoped locks)
     * @param bus    Event bus for publishing state change events
     */
    virtual void onTick(SimulationState& state, EventBus& bus) = 0;

    /**
     * Reset module to initial state. Called on simulation reset.
     */
    virtual void reset() = 0;

    /**
     * Bootstrap module after construction or reset. Called by the
     * ClockController to initialise runtime-dependent state (e.g.
     * frame table). Default is no-op; override if needed.
     *
     * @param state  Shared simulation state to bootstrap into
     * @param frameCount  Number of physical frames (used by MemoryManager)
     */
    virtual void bootstrap(SimulationState& state, uint32_t frameCount) {
        (void)state;
        (void)frameCount;
    }

    /**
     * Get the current operational status of this module.
     */
    virtual ModuleStatus getStatus() const = 0;

    /**
     * Get the human-readable name of this module.
     */
    virtual std::string getModuleName() const = 0;

    /**
     * Virtual destructor for proper cleanup through base pointer.
     */
    virtual ~ISimModule() = default;
};
