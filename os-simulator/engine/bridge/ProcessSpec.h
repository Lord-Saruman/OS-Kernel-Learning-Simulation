#pragma once

/**
 * ProcessSpec.h — REST Payload for Process Creation
 *
 * Reference: DataDictionary_MiniOS_Simulator.md, Section 8.2
 * Received via: POST /process/create
 *
 * JSON payload sent by the React frontend when a user manually creates
 * a process. The API bridge deserialises into ProcessSpec and passes
 * it to the Process Manager.
 */

#include <string>
#include <cstdint>

#include "core/SimEnums.h"

struct ProcessSpec {
    std::string   name;               // Optional user name (empty → auto "proc_N")
    ProcessType   type;               // CPU_BOUND | IO_BOUND | MIXED
    int           priority;           // 1 (highest) to 10 (lowest)
    uint32_t      cpuBurst;           // Total CPU ticks (0 = auto-assign by type)
    uint32_t      ioBurstDuration;    // I/O burst duration (0 for CPU_BOUND)
    uint32_t      memoryRequirement;  // Virtual pages needed (0 = auto-assign)

    // ── Default Constructor ──────────────────────────────────
    ProcessSpec()
        : name("")
        , type(ProcessType::CPU_BOUND)
        , priority(5)
        , cpuBurst(0)
        , ioBurstDuration(0)
        , memoryRequirement(0)
    {}
};
