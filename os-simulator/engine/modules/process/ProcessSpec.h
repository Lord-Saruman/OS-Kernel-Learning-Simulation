#pragma once

/**
 * ProcessSpec.h — Process Creation Specification (REST Payload)
 *
 * Reference: DataDictionary_MiniOS_Simulator.md, Section 8.2
 * Location:  engine/modules/process/ProcessSpec.h
 * Received:  via POST /process/create (deserialized by API Bridge)
 *
 * The JSON payload sent by the React frontend when a user manually creates
 * a process. The API bridge deserialises this into a ProcessSpec and passes
 * it to the Process Manager.
 */

#include <string>
#include <cstdint>

#include "core/SimEnums.h"

struct ProcessSpec {
    std::string   name;               // Optional user-supplied name (empty = auto "proc_N")
    ProcessType   type;               // CPU_BOUND | IO_BOUND | MIXED
    int           priority;           // 1 (highest) to 10 (lowest)
    uint32_t      cpuBurst;           // Total CPU ticks (0 = auto-assign based on type)
    uint32_t      ioBurstDuration;    // I/O burst duration (0 = auto-assign based on type)
    uint32_t      memoryRequirement;  // Virtual pages needed (0 = auto-assign)
    uint32_t      cpuSegmentLength;   // CPU ticks before I/O burst (0 = auto-assign)

    // ── Default Constructor ──────────────────────────────────
    ProcessSpec()
        : name("")
        , type(ProcessType::CPU_BOUND)
        , priority(5)
        , cpuBurst(0)
        , ioBurstDuration(0)
        , memoryRequirement(0)
        , cpuSegmentLength(0)
    {}
};
