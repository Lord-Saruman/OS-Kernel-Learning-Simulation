#pragma once

/**
 * SimEvent.h — Simulation Event for the Event Bus
 *
 * Reference: DataDictionary_MiniOS_Simulator.md, Section 8.1
 * Published by: All modules
 * Consumed by:  API Bridge (for WebSocket broadcast)
 *
 * Every notable state change is published as a SimEvent.
 * The API bridge collects tick events and includes them in the WebSocket packet.
 */

#include <string>
#include <cstdint>

struct SimEvent {
    uint64_t    tick;           // Clock tick when event occurred
    std::string eventType;     // Event type identifier (matches SDD event table)
    int         sourcePid;     // PID of causing process (-1 for system events)
    int         targetPid;     // PID of affected process (-1 if N/A)
    int         resourceId;    // Mutex/semaphore/page ID (-1 if N/A)
    std::string description;   // Human-readable description (max 200 chars)

    // ── Default Constructor ──────────────────────────────────
    SimEvent()
        : tick(0)
        , eventType("")
        , sourcePid(-1)
        , targetPid(-1)
        , resourceId(-1)
        , description("")
    {}

    SimEvent(uint64_t t, const std::string& type, int src, int tgt, int res, const std::string& desc)
        : tick(t)
        , eventType(type)
        , sourcePid(src)
        , targetPid(tgt)
        , resourceId(res)
        , description(desc)
    {}
};

// ─────────────────────────────────────────────────────────────
// Event Type Constants — matches SDD Section 4.1 event table
// Use these constants instead of raw strings to prevent typos.
// ─────────────────────────────────────────────────────────────
namespace EventTypes {
    inline constexpr const char* PROCESS_CREATED       = "PROCESS_CREATED";
    inline constexpr const char* PROCESS_STATE_CHANGED = "PROCESS_STATE_CHANGED";
    inline constexpr const char* PROCESS_TERMINATED    = "PROCESS_TERMINATED";
    inline constexpr const char* CPU_SCHEDULED         = "CPU_SCHEDULED";
    inline constexpr const char* CONTEXT_SWITCH        = "CONTEXT_SWITCH";
    inline constexpr const char* LOCK_ACQUIRED         = "LOCK_ACQUIRED";
    inline constexpr const char* LOCK_RELEASED         = "LOCK_RELEASED";
    inline constexpr const char* PROCESS_BLOCKED       = "PROCESS_BLOCKED";
    inline constexpr const char* PROCESS_UNBLOCKED     = "PROCESS_UNBLOCKED";
    inline constexpr const char* PAGE_FAULT            = "PAGE_FAULT";
    inline constexpr const char* PAGE_REPLACED         = "PAGE_REPLACED";
    inline constexpr const char* TICK_ADVANCED         = "TICK_ADVANCED";
}
